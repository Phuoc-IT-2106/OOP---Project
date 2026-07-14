#include "client/llm_client.h"
#include "harness/evaluator.h"
#include "tools/exec_tool.h"
#include "tools/file_tool.h"
#include "tools/tool_registry.h"
#include "tools/web_search_tool.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using oop_agent::tools::ExecTool;
using oop_agent::tools::FileToolConfig;
using oop_agent::tools::ReadFileTool;
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

} // namespace

int main() {
    try {
        testAbstractInterfaces();
        testRegistryAndPolicy();
        testExecTool();
        testFileToolsThroughRegistry();
        testWebSearchToolWithInjectedTransport();
        std::cout << "All tool tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
