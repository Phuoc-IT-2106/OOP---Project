#pragma once

#include "tool.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace oop_agent::tools {

class ScreenshotTool final : public Tool {
  public:
    explicit ScreenshotTool(
        std::filesystem::path output_directory =
            "output/screenshots"
    );

    std::string_view name() const noexcept override;

    std::string_view description() const noexcept override;

    ToolResult execute(
        const ToolArguments &arguments
    ) override;

  private:
    std::filesystem::path output_directory_;

    static bool isSafeFileName(
        const std::string &file_name
    );

    static std::string createDefaultFileName();
};

} // namespace oop_agent::tools
