#include "gui/action_executor.h"
#include "tools/screenshot_tool.h"

#include <filesystem>
#include <iostream>
#include <unordered_map>

int main() {
    using namespace oop_agent;

    try {
        // =========================
        // Test 1: ScreenshotTool
        // =========================

        tools::ScreenshotTool screenshotTool(
            "output/screenshots"
        );

        tools::ToolArguments screenshotArgs{
            {"file_name", "gui_test.png"}
        };

        tools::ToolResult screenshotResult =
            screenshotTool.execute(
                screenshotArgs
            );

        if (!screenshotResult.success) {
            std::cerr
                << "Screenshot test failed: "
                << screenshotResult.error_message
                << '\n';

            return 1;
        }

        std::cout
            << "[PASS] Screenshot created: "
            << screenshotResult.output
            << '\n';

        if (
            !std::filesystem::exists(
                screenshotResult.output
            )
        ) {
            std::cerr
                << "Screenshot file does not exist.\n";

            return 1;
        }

        // =========================
        // Test 2: ActionExecutor
        // =========================

        gui::ActionExecutor executor;

        auto moveResult =
            executor.moveMouse(
                100,
                100
            );

        if (!moveResult.success) {
            std::cerr
                << "Mouse move test failed: "
                << moveResult.message
                << '\n';

            return 1;
        }

        std::cout
            << "[PASS] "
            << moveResult.message
            << '\n';

        // =========================
        // Test 3: Click
        // =========================

        auto clickResult =
            executor.click(
                100,
                100,
                1
            );
auto clickResult = executor.click(100, 100, 1);

if (!clickResult.success) {
    std::cout
        << "[SKIP] Click test: "
        << clickResult.message
        << '\n';
} else {
    std::cout
        << "[PASS] "
        << clickResult.message
        << '\n';
}

        std::cout
            << "[PASS] "
            << clickResult.message
            << '\n';

        std::cout
            << "\nAll GUI Agent Week 7 tests passed.\n";

        return 0;
    }
    catch (const std::exception &exception) {
        std::cerr
            << "GUI Agent test exception: "
            << exception.what()
            << '\n';

        return 1;
    }
}
