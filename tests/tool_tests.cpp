#include "client/llm_client.h"
#include "harness/evaluator.h"
#include "tools/calculator_tool.h"
#include "tools/exec_tool.h"
#include "tools/file_tool.h"
#include "tools/memory_tool.h"
#include "tools/tool_registry.h"
#include "tools/web_search_tool.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using oop_agent::tools::ExecTool;
using oop_agent::tools::CalculatorTool;
using oop_agent::tools::FileToolConfig;
using oop_agent::tools::ReadFileTool;
using oop_agent::tools::MemoryEntry;
using oop_agent::tools::MemorySaveResponse;
using oop_agent::tools::MemorySaveTool;
using oop_agent::tools::MemorySearchResponse;
using oop_agent::tools::MemorySearchTool;
using oop_agent::tools::MemoryStore;
using oop_agent::tools::MemoryToolConfig;
using oop_agent::tools::makeSqliteMemoryStore;
using oop_agent::tools::ToolRegistry;
using oop_agent::tools::WriteFileTool;
using oop_agent::tools::WebSearchConfig;
using oop_agent::tools::WebSearchRequest;
using oop_agent::tools::WebSearchResponse;
using oop_agent::tools::WebSearchResult;
using oop_agent::tools::WebSearchTool;

void expect(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testAbstractInterfaces() {
    static_assert(std::is_abstract_v<oop_agent::tools::Tool>);
    static_assert(std::is_abstract_v<oop_agent::client::LLMClient>);
    static_assert(std::is_abstract_v<oop_agent::harness::Evaluator>);
}

void testRegistryAndPolicy() {
    ToolRegistry registry;
    expect(registry.registerFactory("exec", [] { return std::make_unique<ExecTool>(); }),
           "first registration should succeed");
    expect(!registry.registerFactory("exec", [] { return std::make_unique<ExecTool>(); }),
           "duplicate registration should be rejected");
    expect(registry.contains("exec"), "registry should contain exec");
    expect(registry.create("exec") != nullptr, "registered tool should be created by name");

    registry.allowOnly({"read_file"});
    expect(registry.create("exec") == nullptr, "allow-list should block exec");
    expect(!registry.execute("exec", {{"command", "echo blocked"}}).success,
           "policy-blocked execution should fail");

    registry.clearPolicy();
    registry.deny({"exec"});
    expect(!registry.isAllowed("exec"), "deny-list should block exec");
    registry.clearPolicy();
    expect(registry.isAllowed("exec"), "clearing policy should restore access");
}

void testExecTool() {
    ExecTool tool;
    const auto missing = tool.execute({});
    expect(!missing.success, "exec should reject a missing command");

    const auto result = tool.execute({{"command", "echo exec-ok"}});
    expect(result.success, "echo command should succeed");
    expect(result.exit_code.has_value() && *result.exit_code == 0,
           "successful command should report exit code 0");
    expect(result.output.find("exec-ok") != std::string::npos,
           "exec should capture command output");

#ifdef _WIN32
    const std::string failing_command =
        "cmd /C \"echo exec-error 1>&2 & exit /B 7\"";
#else
    const std::string failing_command = "sh -c 'echo exec-error >&2; exit 7'";
#endif
    const auto failure = tool.execute({{"command", failing_command}});
    expect(!failure.success, "non-zero command should fail");
    expect(failure.exit_code.has_value() && *failure.exit_code == 7,
           "exec should preserve a non-zero exit code");
    expect(failure.output.find("exec-error") != std::string::npos,
           "exec should capture stderr in its output");
}

void testCalculatorToolThroughRegistry() {
    ToolRegistry registry;
    expect(registry.registerFactory(
               "calculator", [] { return std::make_unique<CalculatorTool>(); }),
           "calculator registration should succeed");

    expect(!registry.execute("calculator", {}).success,
           "calculator should require an expression");

    const auto precedence = registry.execute(
        "calculator", {{"expression", "2 + 3 * 4"}});
    expect(precedence.success && precedence.output == "14",
           "calculator should respect operator precedence");

    const auto parentheses = registry.execute(
        "calculator", {{"expression", "(2 + 3) * -4 / 2"}});
    expect(parentheses.success && parentheses.output == "-10",
           "calculator should support parentheses and unary minus");

    expect(!registry.execute("calculator", {{"expression", "1 / (2 - 2)"}})
                .success,
           "calculator should reject division by zero");
    expect(!registry.execute("calculator", {{"expression", "2 +"}}).success,
           "calculator should reject incomplete expressions");
}

void testFileToolsThroughRegistry() {
    namespace fs = std::filesystem;
    const fs::path workspace = fs::current_path() / "tool-test-workspace";
    fs::remove_all(workspace);
    fs::create_directories(workspace);

    FileToolConfig config;
    config.root_directory = workspace;

    ToolRegistry registry;
    expect(registry.registerFactory(
               "read_file", [config] { return std::make_unique<ReadFileTool>(config); }),
           "read_file registration should succeed");
    expect(registry.registerFactory(
               "write_file", [config] { return std::make_unique<WriteFileTool>(config); }),
           "write_file registration should succeed");

    const auto write = registry.execute(
        "write_file", {{"path", "nested/result.txt"}, {"content", "first"}});
    expect(write.success, "write_file should create parent directories and write data");

    const auto append = registry.execute("write_file",
                                         {{"path", "nested/result.txt"},
                                          {"content", "-second"},
                                          {"append", "true"}});
    expect(append.success, "write_file should support append mode");

    const auto read = registry.execute("read_file", {{"path", "nested/result.txt"}});
    expect(read.success && read.output == "first-second",
           "read_file should return the exact written contents");

    const auto traversal = registry.execute(
        "write_file", {{"path", "../outside.txt"}, {"content", "blocked"}});
    expect(!traversal.success, "write_file should block paths outside workspace root");

    fs::remove_all(workspace);
}

void testWebSearchToolWithInjectedTransport() {
    WebSearchConfig config;
    config.default_max_results = 2;
    config.max_results_limit = 3;
    config.language = "vi";

    WebSearchRequest captured_request;
    const WebSearchTool::SearchTransport transport =
        [&captured_request](const WebSearchRequest &request) {
            captured_request = request;
            return WebSearchResponse{
                200,
                {{"Registry pattern", "https://example.com/registry", "Factory lookup."},
                 {"C++ Tool", "https://example.com/tool", "Abstract command."},
                 {"Ignored", "https://example.com/ignored", "Past requested limit."}},
                {}};
        };

    ToolRegistry registry;
    expect(registry.registerFactory("web_search", [config, transport] {
               return std::make_unique<WebSearchTool>(config, transport);
           }),
           "web_search registration should succeed");

    expect(!registry.execute("web_search", {}).success,
           "web_search should require a query");
    expect(!registry.execute("web_search",
                             {{"query", "C++ registry"}, {"max_results", "4"}})
                .success,
           "web_search should enforce its result limit");

    const auto result = registry.execute(
        "web_search", {{"query", "C++ registry"}, {"max_results", "2"}});
    expect(result.success, "injected web search should succeed");
    expect(captured_request.query == "C++ registry" &&
               captured_request.language == "vi" && captured_request.max_results == 2,
           "web_search should pass validated options to its transport");
    expect(result.output.find("Registry pattern") != std::string::npos &&
               result.output.find("https://example.com/tool") != std::string::npos,
           "web_search should format titles, URLs, and snippets");
    expect(result.output.find("Ignored") == std::string::npos,
           "web_search should not return more than max_results");

    WebSearchTool failing_tool(config, [](const WebSearchRequest &) {
        return WebSearchResponse{503, {}, {}};
    });
    const auto failure = failing_tool.execute({{"query", "unavailable"}});
    expect(!failure.success && failure.error_message.find("503") != std::string::npos,
           "web_search should report non-success HTTP status");
}

class InMemoryStore final : public MemoryStore {
  public:
    MemorySaveResponse save(const std::string &content,
                            const std::string &tags) override {
        entries_.push_back({next_id_, content, tags, "2026-07-14 12:00:00"});
        return {true, next_id_++, {}};
    }

    MemorySearchResponse search(const std::string &query,
                                std::size_t limit) const override {
        MemorySearchResponse response;
        response.success = true;
        for (auto entry = entries_.rbegin();
             entry != entries_.rend() && response.entries.size() < limit;
             ++entry) {
            if (entry->content.find(query) != std::string::npos ||
                entry->tags.find(query) != std::string::npos) {
                response.entries.push_back(*entry);
            }
        }
        return response;
    }

  private:
    std::vector<MemoryEntry> entries_;
    std::int64_t next_id_{1};
};

void testMemoryToolsThroughRegistry() {
    auto store = std::make_shared<InMemoryStore>();
    MemoryToolConfig config;
    config.default_search_limit = 2;
    config.max_search_limit = 3;

    ToolRegistry registry;
    expect(registry.registerFactory("memory_save", [store, config] {
               return std::make_unique<MemorySaveTool>(store, config);
           }),
           "memory_save registration should succeed");
    expect(registry.registerFactory("memory_search", [store, config] {
               return std::make_unique<MemorySearchTool>(store, config);
           }),
           "memory_search registration should succeed");

    expect(!registry.execute("memory_save", {}).success,
           "memory_save should require content");
    const auto first = registry.execute(
        "memory_save", {{"content", "ToolRegistry uses factories"}, {"tags", "oop,registry"}});
    const auto second = registry.execute(
        "memory_save", {{"content", "SQLite stores persistent memory"}, {"tags", "database"}});
    expect(first.success && second.success,
           "memory_save should preserve entries through a shared store");

    const auto by_content = registry.execute("memory_search", {{"query", "SQLite"}});
    expect(by_content.success &&
               by_content.output.find("SQLite stores persistent memory") != std::string::npos,
           "memory_search should find content");
    const auto by_tag = registry.execute("memory_search", {{"query", "registry"}});
    expect(by_tag.success && by_tag.output.find("ToolRegistry uses factories") !=
                                 std::string::npos,
           "memory_search should find tags and format metadata");
    expect(!registry.execute("memory_search", {{"query", "memory"}, {"limit", "4"}})
                .success,
           "memory_search should enforce its configured limit");
}

void testSqliteMemoryStorePersistence() {
    namespace fs = std::filesystem;
    const fs::path workspace = fs::current_path() / "memory-test-workspace";
    fs::remove_all(workspace);

    MemoryToolConfig config;
    config.database_path = workspace / "memory.sqlite3";

    auto first_connection = makeSqliteMemoryStore(config);
    const auto saved = first_connection->save("Progress is 100% complete", "status");
    expect(saved.success && saved.id > 0, "SQLite memory store should save an entry");

    // A new store object opens a new SQLite connection. Finding the old entry
    // proves that memory is persisted on disk rather than held by the Tool.
    auto reopened_store = makeSqliteMemoryStore(config);
    const auto found = reopened_store->search("%", 5);
    expect(found.success && found.entries.size() == 1 &&
               found.entries.front().content == "Progress is 100% complete",
           "SQLite memory should persist and escape LIKE wildcard characters");

    fs::remove_all(workspace);
}

} // namespace

int main() {
    try {
        testAbstractInterfaces();
        testRegistryAndPolicy();
        testExecTool();
        testCalculatorToolThroughRegistry();
        testFileToolsThroughRegistry();
        testWebSearchToolWithInjectedTransport();
        testMemoryToolsThroughRegistry();
        testSqliteMemoryStorePersistence();
        std::cout << "All tool tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
