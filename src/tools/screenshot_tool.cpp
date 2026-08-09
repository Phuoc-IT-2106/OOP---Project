#include "screenshot_tool.h"

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

namespace oop_agent::tools {

ScreenshotTool::ScreenshotTool(
    std::filesystem::path output_directory
)
    : output_directory_(
          std::move(output_directory)
      ) {
}

std::string_view
ScreenshotTool::name() const noexcept {
    return "capture_screenshot";
}

std::string_view
ScreenshotTool::description() const noexcept {
    return
        "Capture the current Linux desktop screen and save it "
        "as a PNG file. Optional argument: file_name.";
}

bool ScreenshotTool::isSafeFileName(
    const std::string &file_name
) {
    if (file_name.empty()) {
        return false;
    }

    if (file_name.find("..") != std::string::npos) {
        return false;
    }

    for (unsigned char character : file_name) {
        const bool valid =
            std::isalnum(character) ||
            character == '_' ||
            character == '-' ||
            character == '.';

        if (!valid) {
            return false;
        }
    }

    return true;
}

std::string
ScreenshotTool::createDefaultFileName() {
    const auto now =
        std::chrono::system_clock::now()
            .time_since_epoch();

    const auto milliseconds =
        std::chrono::duration_cast<
            std::chrono::milliseconds
        >(now).count();

    return
        "screenshot_" +
        std::to_string(milliseconds) +
        ".png";
}

ToolResult ScreenshotTool::execute(
    const ToolArguments &arguments
) {
    const char *display =
        std::getenv("DISPLAY");

    if (display == nullptr ||
        std::string(display).empty()) {
        return ToolResult::failed(
            "DISPLAY is not set. "
            "Screenshot requires an X11 desktop session."
        );
    }

    std::string file_name =
        createDefaultFileName();

    auto file_iterator =
        arguments.find("file_name");

    if (file_iterator != arguments.end() &&
        !file_iterator->second.empty()) {
        file_name = file_iterator->second;
    }

    if (
        file_name.size() < 4 ||
        file_name.substr(
            file_name.size() - 4
        ) != ".png"
    ) {
        file_name += ".png";
    }

    if (!isSafeFileName(file_name)) {
        return ToolResult::failed(
            "Invalid screenshot file name."
        );
    }

    try {
        std::filesystem::create_directories(
            output_directory_
        );
    }
    catch (const std::filesystem::filesystem_error &error) {
        return ToolResult::failed(
            "Cannot create screenshot directory: "
            + std::string(error.what())
        );
    }

    const std::filesystem::path output_path =
        std::filesystem::absolute(
            output_directory_ / file_name
        );

    const std::string command =
        "scrot --overwrite \"" +
        output_path.string() +
        "\"";

    const int exit_code =
        std::system(command.c_str());

    if (exit_code != 0) {
        return ToolResult::failed(
            "scrot failed to capture the screen.",
            "",
            exit_code
        );
    }

    if (!std::filesystem::exists(output_path)) {
        return ToolResult::failed(
            "Screenshot command finished but no image was created."
        );
    }

    return ToolResult::ok(
        output_path.string(),
        exit_code
    );
}

} // namespace oop_agent::tools
