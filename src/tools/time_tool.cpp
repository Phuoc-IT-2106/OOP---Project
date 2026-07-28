#include "time_tool.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace oop_agent::tools {

std::string_view TimeTool::name() const noexcept {
    return "time";
}

std::string_view TimeTool::description() const noexcept {
    return "Return the current local date and time in ISO-8601 format. No arguments.";
}

ToolResult TimeTool::execute(const ToolArguments &) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#ifdef _WIN32
    if (localtime_s(&local_time, &raw_time) != 0) {
        return ToolResult::failed("could not convert current local time");
    }
#else
    if (localtime_r(&raw_time, &local_time) == nullptr) {
        return ToolResult::failed("could not convert current local time");
    }
#endif

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S");
    return ToolResult::ok(output.str());
}

} // namespace oop_agent::tools
