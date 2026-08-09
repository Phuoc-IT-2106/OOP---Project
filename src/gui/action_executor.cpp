#pragma once

#include "gui_action.h"

#include <string>

struct xdo;

namespace oop_agent::gui {

struct ActionExecutionResult {
    bool success{false};
    std::string message;
};

class ActionExecutor {
  public:
    ActionExecutor();
    ~ActionExecutor();

    ActionExecutor(const ActionExecutor &) = delete;
    ActionExecutor &operator=(const ActionExecutor &) = delete;

    ActionExecutionResult execute(
        const GuiAction &action
    );

    ActionExecutionResult moveMouse(
        int x,
        int y
    );

    ActionExecutionResult click(
        int x,
        int y,
        int button = 1
    );

    ActionExecutionResult typeText(
        const std::string &text
    );

    ActionExecutionResult keyPress(
        const std::string &key
    );

  private:
    xdo *xdo_context_{nullptr};
};

} // namespace oop_agent::gui
