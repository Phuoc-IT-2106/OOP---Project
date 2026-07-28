#include "text_stats_tool.h"

#include <cctype>
#include <string>

namespace oop_agent::tools {

std::string_view TextStatsTool::name() const noexcept {
    return "text_stats";
}

std::string_view TextStatsTool::description() const noexcept {
    return "Count lines, words, and bytes in text. Argument: text.";
}

ToolResult TextStatsTool::execute(const ToolArguments &arguments) {
    const auto found = arguments.find("text");
    if (found == arguments.end()) {
        return ToolResult::failed("text_stats requires a 'text' argument");
    }

    const std::string &text = found->second;
    std::size_t lines = text.empty() ? 0 : 1;
    std::size_t words = 0;
    bool inside_word = false;
    for (const unsigned char character : text) {
        if (character == '\n') {
            ++lines;
        }
        const bool whitespace = std::isspace(character) != 0;
        if (!whitespace && !inside_word) {
            ++words;
        }
        inside_word = !whitespace;
    }
    if (!text.empty() && text.back() == '\n') {
        --lines;
    }

    return ToolResult::ok("lines=" + std::to_string(lines) +
                          ", words=" + std::to_string(words) +
                          ", bytes=" + std::to_string(text.size()));
}

} // namespace oop_agent::tools
