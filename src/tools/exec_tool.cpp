#include "exec_tool.h"

#include <array>
#include <cstdio>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace oop_agent::tools {
namespace {

FILE *openPipe(const std::string &command) {
#ifdef _WIN32
    return _popen(command.c_str(), "r");
#else
    return popen(command.c_str(), "r");
#endif
}

int closePipe(FILE *pipe) {
#ifdef _WIN32
    return _pclose(pipe);
#else
    const int status = pclose(pipe);
    if (status == -1) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return status;
#endif
}

} // namespace

std::string_view ExecTool::name() const noexcept {
    return "exec";
}

std::string_view ExecTool::description() const noexcept {
    return "Run a shell command. Argument: command. Returns combined stdout/stderr and exit code.";
}

ToolResult ExecTool::execute(const ToolArguments &arguments) {
    const auto command_argument = arguments.find("command");
    if (command_argument == arguments.end() || command_argument->second.empty()) {
        return ToolResult::failed("exec requires a non-empty 'command' argument");
    }

    // stderr is redirected into stdout so the agent receives diagnostic output
    // even when the command fails. The command is intentionally interpreted by
    // the host shell; command-level isolation belongs to the deployment sandbox.
    const std::string shell_command = command_argument->second + " 2>&1";
    FILE *pipe = openPipe(shell_command);
    if (pipe == nullptr) {
        return ToolResult::failed("could not start the shell command");
    }

    std::string output;
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output.append(buffer.data());
    }

    const int exit_code = closePipe(pipe);
    if (exit_code == -1) {
        return ToolResult::failed("could not collect the shell command status", output);
    }
    if (exit_code != 0) {
        return ToolResult::failed("shell command exited with code " +
                                      std::to_string(exit_code),
                                  output,
                                  exit_code);
    }
    return ToolResult::ok(std::move(output), exit_code);
}

} // namespace oop_agent::tools
