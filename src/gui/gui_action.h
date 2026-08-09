#pragma once

#include <string>
#include <variant>

namespace oop_agent::gui {

struct ClickAction {
    int x{0};
    int y{0};
    int button{1};
};

struct TypeTextAction {
    std::string text;
};

struct KeyPressAction {
    std::string key;
};

struct MoveMouseAction {
    int x{0};
    int y{0};
};

struct DoneAction {
    std::string message;
};

using GuiAction = std::variant<
    ClickAction,
    TypeTextAction,
    KeyPressAction,
    MoveMouseAction,
    DoneAction
>;

} // namespace oop_agent::gui
