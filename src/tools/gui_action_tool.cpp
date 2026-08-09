#include "gui_action_tool.h"

#include <exception>
#include <string>

namespace oop_agent::tools {

GuiActionTool::GuiActionTool(
    gui::ActionExecutor &executor
)
    : executor_(executor) {
}

std::string_view
GuiActionTool::name() const noexcept {
    return "gui_action";
}

std::string_view
GuiActionTool::description() const noexcept {
    return
        "Execute a Linux GUI action using libxdo. "
        "Supported actions: "
        "move_mouse(x,y), click(x,y,button), "
        "type_text(text), key_press(key).";
}

bool GuiActionTool::parseInteger(
    const std::string &text,
    int &value
) {
    try {
        std::size_t parsed_characters = 0;

        value = std::stoi(
            text,
            &parsed_characters
        );

        return parsed_characters == text.size();
    }
    catch (...) {
        return false;
    }
}

ToolResult GuiActionTool::execute(
    const ToolArguments &arguments
) {
    const auto action_iterator =
        arguments.find("action");

    if (action_iterator == arguments.end()) {
        return ToolResult::failed(
            "Missing required argument: action."
        );
    }

    const std::string &action =
        action_iterator->second;

    try {

        // =========================
        // MOVE MOUSE
        // =========================

        if (action == "move_mouse") {
            const auto x_iterator =
                arguments.find("x");

            const auto y_iterator =
                arguments.find("y");

            if (
                x_iterator == arguments.end() ||
                y_iterator == arguments.end()
            ) {
                return ToolResult::failed(
                    "move_mouse requires x and y."
                );
            }

            int x = 0;
            int y = 0;

            if (
                !parseInteger(
                    x_iterator->second,
                    x
                ) ||
                !parseInteger(
                    y_iterator->second,
                    y
                )
            ) {
                return ToolResult::failed(
                    "x and y must be integers."
                );
            }

            const gui::ActionExecutionResult result =
                executor_.moveMouse(x, y);

            if (!result.success) {
                return ToolResult::failed(
                    result.message
                );
            }

            return ToolResult::ok(
                result.message
            );
        }

        // =========================
        // CLICK
        // =========================

        if (action == "click") {
            const auto x_iterator =
                arguments.find("x");

            const auto y_iterator =
                arguments.find("y");

            if (
                x_iterator == arguments.end() ||
                y_iterator == arguments.end()
            ) {
                return ToolResult::failed(
                    "click requires x and y."
                );
            }

            int x = 0;
            int y = 0;
            int button = 1;

            if (
                !parseInteger(
                    x_iterator->second,
                    x
                ) ||
                !parseInteger(
                    y_iterator->second,
                    y
                )
            ) {
                return ToolResult::failed(
                    "x and y must be integers."
                );
            }

            const auto button_iterator =
                arguments.find("button");

            if (
                button_iterator != arguments.end() &&
                !parseInteger(
                    button_iterator->second,
                    button
                )
            ) {
                return ToolResult::failed(
                    "button must be an integer."
                );
            }

            if (button < 1 || button > 5) {
                return ToolResult::failed(
                    "button must be between 1 and 5."
                );
            }

            const gui::ActionExecutionResult result =
                executor_.click(
                    x,
                    y,
                    button
                );

            if (!result.success) {
                return ToolResult::failed(
                    result.message
                );
            }

            return ToolResult::ok(
                result.message
            );
        }

        // =========================
        // TYPE TEXT
        // =========================

        if (action == "type_text") {
            const auto text_iterator =
                arguments.find("text");

            if (text_iterator == arguments.end()) {
                return ToolResult::failed(
                    "type_text requires text."
                );
            }

            const gui::ActionExecutionResult result =
                executor_.typeText(
                    text_iterator->second
                );

            if (!result.success) {
                return ToolResult::failed(
                    result.message
                );
            }

            return ToolResult::ok(
                result.message
            );
        }

        // =========================
        // KEY PRESS
        // =========================

        if (action == "key_press") {
            const auto key_iterator =
                arguments.find("key");

            if (key_iterator == arguments.end()) {
                return ToolResult::failed(
                    "key_press requires key."
                );
            }

            const gui::ActionExecutionResult result =
                executor_.keyPress(
                    key_iterator->second
                );

            if (!result.success) {
                return ToolResult::failed(
                    result.message
                );
            }

            return ToolResult::ok(
                result.message
            );
        }

        return ToolResult::failed(
            "Unsupported GUI action: " +
            action
        );
    }
    catch (const std::exception &error) {
        return ToolResult::failed(
            "GUI action failed: " +
            std::string(error.what())
        );
    }
}

} // namespace oop_agent::tools
