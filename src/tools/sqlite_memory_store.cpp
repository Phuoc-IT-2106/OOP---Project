#include "memory_tool.h"

#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <sqlite3.h>

namespace oop_agent::tools {
namespace {

struct DatabaseCloser {
    void operator()(sqlite3 *database) const {
        if (database != nullptr) {
            sqlite3_close_v2(database);
        }
    }
};

struct StatementCloser {
    void operator()(sqlite3_stmt *statement) const {
        if (statement != nullptr) {
            sqlite3_finalize(statement);
        }
    }
};

using Database = std::unique_ptr<sqlite3, DatabaseCloser>;
using Statement = std::unique_ptr<sqlite3_stmt, StatementCloser>;

std::string sqliteError(sqlite3 *database, const std::string &prefix) {
    return prefix + ": " + (database == nullptr ? "unknown SQLite error"
                                                : sqlite3_errmsg(database));
}

Database openDatabase(const MemoryToolConfig &config) {
    const auto parent = config.database_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    sqlite3 *raw_database = nullptr;
    const std::string path = config.database_path.u8string();
    const int status = sqlite3_open_v2(path.c_str(),
                                       &raw_database,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                           SQLITE_OPEN_FULLMUTEX,
                                       nullptr);
    Database database(raw_database);
    if (status != SQLITE_OK || database == nullptr) {
        throw std::runtime_error(sqliteError(database.get(), "could not open memory database"));
    }
    if (sqlite3_busy_timeout(database.get(), config.busy_timeout_ms) != SQLITE_OK) {
        throw std::runtime_error(
            sqliteError(database.get(), "could not configure SQLite busy timeout"));
    }
    return database;
}

void executeSchema(sqlite3 *database) {
    constexpr const char *schema =
        "CREATE TABLE IF NOT EXISTS memories ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "content TEXT NOT NULL,"
        "tags TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_memories_created_at "
        "ON memories(created_at DESC);";

    char *raw_error = nullptr;
    const int status = sqlite3_exec(database, schema, nullptr, nullptr, &raw_error);
    if (status != SQLITE_OK) {
        const std::string message = raw_error == nullptr ? sqlite3_errmsg(database) : raw_error;
        sqlite3_free(raw_error);
        throw std::runtime_error("could not initialize memory schema: " + message);
    }
}

Statement prepare(sqlite3 *database, const char *sql) {
    sqlite3_stmt *raw_statement = nullptr;
    const int status = sqlite3_prepare_v2(database, sql, -1, &raw_statement, nullptr);
    Statement statement(raw_statement);
    if (status != SQLITE_OK || statement == nullptr) {
        throw std::runtime_error(sqliteError(database, "could not prepare memory statement"));
    }
    return statement;
}

void bindText(sqlite3 *database,
              sqlite3_stmt *statement,
              int parameter,
              const std::string &value) {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("memory value is too large for SQLite");
    }
    if (sqlite3_bind_text(statement,
                          parameter,
                          value.data(),
                          static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
        throw std::runtime_error(sqliteError(database, "could not bind memory value"));
    }
}

std::string columnText(sqlite3_stmt *statement, int column) {
    const auto *text = sqlite3_column_text(statement, column);
    const int byte_count = sqlite3_column_bytes(statement, column);
    if (text == nullptr || byte_count <= 0) {
        return {};
    }
    return {reinterpret_cast<const char *>(text), static_cast<std::size_t>(byte_count)};
}

// LIKE treats '%' and '_' as wildcards. Escaping them makes an LLM query a
// literal keyword search instead of an accidental pattern language.
std::string escapeLikePattern(const std::string &query) {
    std::string escaped;
    escaped.reserve(query.size());
    for (const char character : query) {
        if (character == '\\' || character == '%' || character == '_') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return "%" + escaped + "%";
}

} // namespace

SqliteMemoryStore::SqliteMemoryStore(MemoryToolConfig config)
    : config_(std::move(config)) {
    if (config_.database_path.empty()) {
        throw std::invalid_argument("memory database_path must not be empty");
    }
    if (config_.busy_timeout_ms < 0) {
        throw std::invalid_argument("memory busy_timeout_ms must not be negative");
    }
    auto database = openDatabase(config_);
    executeSchema(database.get());
}

MemorySaveResponse SqliteMemoryStore::save(const std::string &content,
                                           const std::string &tags) {
    try {
        auto database = openDatabase(config_);
        auto statement = prepare(
            database.get(), "INSERT INTO memories(content, tags) VALUES(?1, ?2);");
        bindText(database.get(), statement.get(), 1, content);
        bindText(database.get(), statement.get(), 2, tags);
        if (sqlite3_step(statement.get()) != SQLITE_DONE) {
            return {false, 0, sqliteError(database.get(), "could not save memory")};
        }
        return {true, sqlite3_last_insert_rowid(database.get()), {}};
    } catch (const std::exception &error) {
        return {false, 0, error.what()};
    }
}

MemorySearchResponse SqliteMemoryStore::search(const std::string &query,
                                               std::size_t limit) const {
    try {
        auto database = openDatabase(config_);
        auto statement = prepare(
            database.get(),
            "SELECT id, content, tags, created_at FROM memories "
            "WHERE content LIKE ?1 ESCAPE '\\' OR tags LIKE ?1 ESCAPE '\\' "
            "ORDER BY id DESC LIMIT ?2;");
        const std::string pattern = escapeLikePattern(query);
        bindText(database.get(), statement.get(), 1, pattern);
        if (limit > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            sqlite3_bind_int(statement.get(), 2, static_cast<int>(limit)) != SQLITE_OK) {
            return {false, {}, "could not bind memory search limit"};
        }

        MemorySearchResponse response;
        response.success = true;
        int status = SQLITE_ROW;
        while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
            response.entries.push_back({sqlite3_column_int64(statement.get(), 0),
                                        columnText(statement.get(), 1),
                                        columnText(statement.get(), 2),
                                        columnText(statement.get(), 3)});
        }
        if (status != SQLITE_DONE) {
            return {false, {}, sqliteError(database.get(), "could not search memory")};
        }
        return response;
    } catch (const std::exception &error) {
        return {false, {}, error.what()};
    }
}

std::shared_ptr<MemoryStore> makeSqliteMemoryStore(MemoryToolConfig config) {
    return std::make_shared<SqliteMemoryStore>(std::move(config));
}

} // namespace oop_agent::tools
