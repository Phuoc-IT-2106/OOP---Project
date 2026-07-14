#pragma once

#include "tool.h"

#include <string_view>

namespace oop_agent::tools {

class CalculatorTool final : public Tool {
  public:
    std::string_view name() const noexcept override;
    std::string_view description() const noexcept override;
    ToolResult execute(const ToolArguments &arguments) override;
};

} // namespace oop_agent::tools
