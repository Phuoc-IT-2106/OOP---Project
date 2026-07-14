#pragma once

#include "tool.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace oop_agent::tools {

struct MemoryToolConfig {
    std::filesystem::path database_path{"data/memory.sqlite3"};
    std::size_t default_search_limit{5};
    std::size_t max_search_limit{20};
    std::size_t max_content_bytes{64 * 1024};
    std::size_t max_tags_bytes{4 * 1024};
    std::size_t max_query_bytes{4 * 1024};
    int busy_timeout_ms{3000};
};

struct MemoryEntry {
    std::int64_t id{0};
    std::string content;
    std::string tags;
    std::string created_at;
};

struct MemorySaveResponse {
    bool success{false};
    std::int64_t id{0};
    std::string error_message;
};

struct MemorySearchResponse {
    bool success{false};
    std::vector<MemoryEntry> entries;
    std::string error_message;
};

// Persistence abstraction: tools know how to validate/format an agent call,
// while a repository decides whether data lives in SQLite, memory, or a future
// vector database. This also keeps unit tests independent from SQLite files.
class MemoryStore {
  public:
    virtual ~MemoryStore() = default;

    virtual MemorySaveResponse save(const std::string &content,
                                    const std::string &tags) = 0;
    virtual MemorySearchResponse search(const std::string &query,
                                        std::size_t limit) const = 0;
};

// SQLite adapter. Each operation opens a short-lived connection, which makes a
// shared store safe to use from separately created Tool instances.
class SqliteMemoryStore final : public MemoryStore {
  public:
    explicit SqliteMemoryStore(MemoryToolConfig config = {});

    MemorySaveResponse save(const std::string &content,
                            const std::string &tags) override;
    MemorySearchResponse search(const std::string &query,
                                std::size_t limit) const override;

  private:
    MemoryToolConfig config_;
};

class MemorySaveTool final : public Tool {
  public:
    MemorySaveTool(std::shared_ptr<MemoryStore> store,
                   MemoryToolConfig config = {});

    std::string_view name() const noexcept override;
    std::string_view description() const noexcept override;
    ToolResult execute(const ToolArguments &arguments) override;

  private:
    std::shared_ptr<MemoryStore> store_;
    MemoryToolConfig config_;
};

class MemorySearchTool final : public Tool {
  public:
    MemorySearchTool(std::shared_ptr<MemoryStore> store,
                     MemoryToolConfig config = {});

    std::string_view name() const noexcept override;
    std::string_view description() const noexcept override;
    ToolResult execute(const ToolArguments &arguments) override;

  private:
    std::shared_ptr<MemoryStore> store_;
    MemoryToolConfig config_;
};

std::shared_ptr<MemoryStore> makeSqliteMemoryStore(MemoryToolConfig config = {});

} // namespace oop_agent::tools
