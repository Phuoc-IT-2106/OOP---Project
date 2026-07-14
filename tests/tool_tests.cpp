#include "client/llm_client.h"
#include "harness/evaluator.h"
#include "tools/exec_tool.h"
#include "tools/file_tool.h"
#include "tools/tool_registry.h"

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

} // namespace

int main() {
    try {
        testAbstractInterfaces();
        testRegistryAndPolicy();
        testExecTool();
        testFileToolsThroughRegistry();
        std::cout << "All tool tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
