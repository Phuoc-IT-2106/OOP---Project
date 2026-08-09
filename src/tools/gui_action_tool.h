#pragma once

#include "gui/action_executor.h"
#include "tool.h"

#include <string_view>

namespace oop_agent::tools {

class GuiActionTool final : public Tool {
  public:
    explicit GuiActionTool(
        gui::ActionExecutor &executor
    );

    std::string_view name() const noexcept override;

    std::string_view description() const noexcept override;

    ToolResult execute(
        const ToolArguments &arguments
    ) override;

  private:
    gui::ActionExecutor &executor_;

    static bool parseInteger(
        const std::string &text,
        int &value
    );
};

} // namespace oop_agent::tools
