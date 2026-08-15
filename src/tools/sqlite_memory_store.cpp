#include "memory_tool.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

void executeSql(sqlite3 *database,
                const char *sql,
                const std::string &error_prefix) {
    char *raw_error = nullptr;
    const int status =
        sqlite3_exec(database, sql, nullptr, nullptr, &raw_error);
    if (status != SQLITE_OK) {
        const std::string message =
            raw_error == nullptr ? sqlite3_errmsg(database) : raw_error;
        sqlite3_free(raw_error);
        throw std::runtime_error(error_prefix + ": " + message);
    }
}

Database openDatabase(const MemoryToolConfig &config) {
    const auto parent = config.database_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    sqlite3 *raw_database = nullptr;
    const std::string path = config.database_path.string();
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
    executeSql(database.get(),
               "PRAGMA foreign_keys = ON;",
               "could not enable SQLite foreign keys");
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
        "ON memories(created_at DESC);"
        "CREATE TABLE IF NOT EXISTS memory_embeddings ("
        "memory_id INTEGER PRIMARY KEY,"
        "vector TEXT NOT NULL,"
        "dimensions INTEGER NOT NULL CHECK(dimensions > 0),"
        "FOREIGN KEY(memory_id) REFERENCES memories(id) ON DELETE CASCADE"
        ");";

    executeSql(database,
               schema,
               "could not initialize memory schema");
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

std::string serializeEmbedding(const std::vector<double> &embedding) {
    if (embedding.empty()) {
        throw std::invalid_argument("embedding vector must not be empty");
    }

    // Text storage is portable and easy to inspect during the course demo.
    // max_digits10 preserves enough precision for a double round trip.
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (std::size_t index = 0; index < embedding.size(); ++index) {
        if (!std::isfinite(embedding[index])) {
            throw std::invalid_argument(
                "embedding vector contains a non-finite coordinate");
        }
        if (index != 0) {
            output << ',';
        }
        output << embedding[index];
    }
    return output.str();
}

std::vector<double> deserializeEmbedding(const std::string &serialized,
                                         std::size_t expected_dimensions) {
    if (serialized.empty() || serialized.back() == ',') {
        throw std::runtime_error("stored embedding vector is malformed");
    }

    std::vector<double> embedding;
    embedding.reserve(expected_dimensions);
    std::istringstream input(serialized);
    std::string coordinate_text;
    while (std::getline(input, coordinate_text, ',')) {
        if (coordinate_text.empty()) {
            throw std::runtime_error("stored embedding vector is malformed");
        }
        std::size_t parsed_characters = 0;
        const double coordinate =
            std::stod(coordinate_text, &parsed_characters);
        if (parsed_characters != coordinate_text.size() ||
            !std::isfinite(coordinate)) {
            throw std::runtime_error("stored embedding coordinate is invalid");
        }
        embedding.push_back(coordinate);
    }
    if (embedding.size() != expected_dimensions) {
        throw std::runtime_error("stored embedding dimension does not match metadata");
    }
    return embedding;
}

class Transaction {
  public:
    explicit Transaction(sqlite3 *database) : database_(database) {
        executeSql(database_,
                   "BEGIN IMMEDIATE;",
                   "could not begin memory transaction");
    }

    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    ~Transaction() {
        if (active_) {
            sqlite3_exec(database_, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
    }

    void commit() {
        executeSql(database_,
                   "COMMIT;",
                   "could not commit memory transaction");
        active_ = false;
    }

  private:
    sqlite3 *database_;
    bool active_{true};
};

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

MemorySaveResponse SqliteMemoryStore::saveWithEmbedding(
    const std::string &content,
    const std::string &tags,
    const std::vector<double> &embedding) {
    try {
        const std::string serialized_embedding =
            serializeEmbedding(embedding);
        if (embedding.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return {false, 0, "embedding dimension is too large for SQLite"};
        }

        auto database = openDatabase(config_);
        Transaction transaction(database.get());

        auto memory_statement = prepare(
            database.get(),
            "INSERT INTO memories(content, tags) VALUES(?1, ?2);");
        bindText(database.get(), memory_statement.get(), 1, content);
        bindText(database.get(), memory_statement.get(), 2, tags);
        if (sqlite3_step(memory_statement.get()) != SQLITE_DONE) {
            return {false,
                    0,
                    sqliteError(database.get(), "could not save memory")};
        }
        const std::int64_t memory_id =
            sqlite3_last_insert_rowid(database.get());

        auto embedding_statement = prepare(
            database.get(),
            "INSERT INTO memory_embeddings(memory_id, vector, dimensions) "
            "VALUES(?1, ?2, ?3);");
        if (sqlite3_bind_int64(embedding_statement.get(), 1, memory_id) !=
            SQLITE_OK) {
            return {false, 0, "could not bind memory embedding id"};
        }
        bindText(database.get(),
                 embedding_statement.get(),
                 2,
                 serialized_embedding);
        if (sqlite3_bind_int(embedding_statement.get(),
                             3,
                             static_cast<int>(embedding.size())) != SQLITE_OK) {
            return {false, 0, "could not bind memory embedding dimensions"};
        }
        if (sqlite3_step(embedding_statement.get()) != SQLITE_DONE) {
            return {false,
                    0,
                    sqliteError(database.get(),
                                "could not save memory embedding")};
        }

        transaction.commit();
        return {true, memory_id, {}};
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
                                        columnText(statement.get(), 3),
                                        std::nullopt});
        }
        if (status != SQLITE_DONE) {
            return {false, {}, sqliteError(database.get(), "could not search memory")};
        }
        return response;
    } catch (const std::exception &error) {
        return {false, {}, error.what()};
    }
}

MemorySearchResponse SqliteMemoryStore::searchSimilar(
    const std::vector<double> &query_embedding,
    std::size_t limit) const {
    try {
        if (limit == 0) {
            return {false, {}, "memory similarity search limit must be positive"};
        }
        // Validate dimensions and coordinates before opening the database.
        (void)cosineSimilarity(query_embedding, query_embedding);

        auto database = openDatabase(config_);
        auto statement = prepare(
            database.get(),
            "SELECT m.id, m.content, m.tags, m.created_at, "
            "e.vector, e.dimensions "
            "FROM memories AS m "
            "INNER JOIN memory_embeddings AS e ON e.memory_id = m.id;");

        MemorySearchResponse response;
        response.success = true;
        int status = SQLITE_ROW;
        while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
            const auto raw_dimensions =
                sqlite3_column_int64(statement.get(), 5);
            if (raw_dimensions <= 0 ||
                static_cast<std::uint64_t>(raw_dimensions) >
                    std::numeric_limits<std::size_t>::max()) {
                continue;
            }

            try {
                const auto stored_embedding = deserializeEmbedding(
                    columnText(statement.get(), 4),
                    static_cast<std::size_t>(raw_dimensions));
                const double similarity =
                    cosineSimilarity(query_embedding, stored_embedding);
                response.entries.push_back(
                    {sqlite3_column_int64(statement.get(), 0),
                     columnText(statement.get(), 1),
                     columnText(statement.get(), 2),
                     columnText(statement.get(), 3),
                     similarity});
            } catch (const std::exception &) {
                // One corrupt or dimension-incompatible row should not make
                // all other persistent memories unsearchable.
            }
        }
        if (status != SQLITE_DONE) {
            return {false,
                    {},
                    sqliteError(database.get(),
                                "could not search memory embeddings")};
        }

        std::sort(
            response.entries.begin(),
            response.entries.end(),
            [](const MemoryEntry &left, const MemoryEntry &right) {
                if (*left.similarity == *right.similarity) {
                    return left.id > right.id;
                }
                return *left.similarity > *right.similarity;
            });
        if (response.entries.size() > limit) {
            response.entries.resize(limit);
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
