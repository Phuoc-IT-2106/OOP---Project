#include "action_executor.h"

#include <stdexcept>
#include <type_traits>

extern "C" {
#include <xdo.h>
}
namespace oop_agent::gui {

ActionExecutor::ActionExecutor() {
    xdo_context_ = xdo_new(nullptr);

    if (xdo_context_ == nullptr) {
        throw std::runtime_error(
            "Cannot initialize libxdo. "
            "Check DISPLAY and X11 session."
        );
    }
}

ActionExecutor::~ActionExecutor() {
    if (xdo_context_ != nullptr) {
        xdo_free(xdo_context_);
        xdo_context_ = nullptr;
    }
}

ActionExecutionResult ActionExecutor::moveMouse(
    int x,
    int y
) {
    if (x < 0 || y < 0) {
        return {
            false,
            "Coordinates must be non-negative."
        };
    }

    const int status =
        xdo_move_mouse(
            xdo_context_,
            x,
            y,
            0
        );

    if (status != XDO_SUCCESS) {
        return {
            false,
            "Failed to move mouse."
        };
    }

    return {
        true,
        "Mouse moved successfully."
    };
}

ActionExecutionResult ActionExecutor::click(
    int x,
    int y,
    int button
) {
    const ActionExecutionResult moveResult =
        moveMouse(x, y);

    if (!moveResult.success) {
        return moveResult;
    }

    Window activeWindow = 0;

    if (
        xdo_get_active_window(
            xdo_context_,
            &activeWindow
        ) != XDO_SUCCESS
    ) {
        return {
            false,
            "Cannot get active window."
        };
    }

    const int status =
        xdo_click_window(
            xdo_context_,
            activeWindow,
            button
        );

    if (status != XDO_SUCCESS) {
        return {
            false,
            "Failed to click mouse."
        };
    }

    return {
        true,
        "Mouse clicked successfully."
    };
}

ActionExecutionResult ActionExecutor::typeText(
    const std::string &text
) {
    if (text.empty()) {
        return {
            false,
            "Text cannot be empty."
        };
    }

    Window activeWindow = 0;

    if (
        xdo_get_active_window(
            xdo_context_,
            &activeWindow
        ) != XDO_SUCCESS
    ) {
        return {
            false,
            "Cannot get active window."
        };
    }

    const int status =
        xdo_enter_text_window(
            xdo_context_,
            activeWindow,
            text.c_str(),
            12000
        );

    if (status != XDO_SUCCESS) {
        return {
            false,
            "Failed to type text."
        };
    }

    return {
        true,
        "Text typed successfully."
    };
}

ActionExecutionResult ActionExecutor::keyPress(
    const std::string &key
) {
    if (key.empty()) {
        return {
            false,
            "Key cannot be empty."
        };
    }

    Window activeWindow = 0;

    if (
        xdo_get_active_window(
            xdo_context_,
            &activeWindow
        ) != XDO_SUCCESS
    ) {
        return {
            false,
            "Cannot get active window."
        };
    }

    const int status =
        xdo_send_keysequence_window(
            xdo_context_,
            activeWindow,
            key.c_str(),
            12000
        );

    if (status != XDO_SUCCESS) {
        return {
            false,
            "Failed to press key."
        };
    }

    return {
        true,
        "Key pressed successfully."
    };
}

ActionExecutionResult ActionExecutor::execute(
    const GuiAction &action
) {
    return std::visit(
        [this](const auto &currentAction)
            -> ActionExecutionResult {

            using ActionType =
                std::decay_t<
                    decltype(currentAction)
                >;

            if constexpr (
                std::is_same_v<
                    ActionType,
                    ClickAction
                >
            ) {
                return click(
                    currentAction.x,
                    currentAction.y,
                    currentAction.button
                );
            }

            else if constexpr (
                std::is_same_v<
                    ActionType,
                    TypeTextAction
                >
            ) {
                return typeText(
                    currentAction.text
                );
            }

            else if constexpr (
                std::is_same_v<
                    ActionType,
                    KeyPressAction
                >
            ) {
                return keyPress(
                    currentAction.key
                );
            }

            else if constexpr (
                std::is_same_v<
                    ActionType,
                    MoveMouseAction
                >
            ) {
                return moveMouse(
                    currentAction.x,
                    currentAction.y
                );
            }

            else if constexpr (
                std::is_same_v<
                    ActionType,
                    DoneAction
                >
            ) {
                return {
                    true,
                    currentAction.message
                };
            }

            return {
                false,
                "Unsupported GUI action."
            };
        },
        action
    );
}

} // namespace oop_agent::gui
