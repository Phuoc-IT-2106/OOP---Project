#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace oop_agent::tools {

// Arguments are named because the LLM sends a JSON-like object to a tool.
// Keeping this type independent from a JSON library makes Tool reusable in tests
// and in a future LLM backend.
using ToolArguments = std::unordered_map<std::string, std::string>;

struct ToolResult {
    bool success{false};
    std::string output;
    std::string error_message;

    // Only process-like tools have an exit code, so optional avoids magic values.
    std::optional<int> exit_code;

    static ToolResult ok(std::string output_value = {},
                         std::optional<int> code = std::nullopt) {
        return {true, std::move(output_value), {}, code};
    }

    static ToolResult failed(std::string error,
                             std::string partial_output = {},
                             std::optional<int> code = std::nullopt) {
        return {false, std::move(partial_output), std::move(error), code};
    }
};

// Command interface: AgentLoop only depends on this abstraction, never on a
// concrete tool such as ExecTool or ReadFileTool.
class Tool {
  public:
    virtual ~Tool() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual std::string_view description() const noexcept = 0;
    virtual ToolResult execute(const ToolArguments &arguments) = 0;
};

} // namespace oop_agent::tools
