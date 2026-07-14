#include "file_tool.h"

#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace oop_agent::tools {
namespace {

struct PathResolution {
    std::filesystem::path path;
    std::string error;
};

PathResolution resolvePath(const FileToolConfig &config, const std::string &raw_path) {
    namespace fs = std::filesystem;

    if (raw_path.empty()) {
        return {{}, "file path must not be empty"};
    }

    try {
        const fs::path root = fs::weakly_canonical(fs::absolute(config.root_directory));
        const fs::path requested(raw_path);
        const fs::path candidate = requested.is_absolute() ? requested : root / requested;
        const fs::path resolved = fs::weakly_canonical(candidate);

        if (config.restrict_to_root) {
            const fs::path relative = resolved.lexically_relative(root);
            const bool different_root = relative.empty() && resolved != root;
            const bool climbs_out = !relative.empty() && *relative.begin() == "..";
            if (different_root || relative.is_absolute() || climbs_out) {
                return {{}, "file path escapes the configured workspace root"};
            }
        }
        return {resolved, {}};
    } catch (const fs::filesystem_error &error) {
        return {{}, std::string("could not resolve file path: ") + error.what()};
    }
}

bool parseAppendFlag(const ToolArguments &arguments, bool &append, std::string &error) {
    const auto argument = arguments.find("append");
    if (argument == arguments.end() || argument->second.empty() ||
        argument->second == "false" || argument->second == "0") {
        append = false;
        return true;
    }
    if (argument->second == "true" || argument->second == "1") {
        append = true;
        return true;
    }
    error = "'append' must be true, false, 1, or 0";
    return false;
}

} // namespace

ReadFileTool::ReadFileTool(FileToolConfig config) : config_(std::move(config)) {}

std::string_view ReadFileTool::name() const noexcept {
    return "read_file";
}

std::string_view ReadFileTool::description() const noexcept {
    return "Read a UTF-8/text or binary file. Argument: path. Returns the file contents.";
}

ToolResult ReadFileTool::execute(const ToolArguments &arguments) {
    const auto path_argument = arguments.find("path");
    if (path_argument == arguments.end()) {
        return ToolResult::failed("read_file requires a 'path' argument");
    }

    const auto resolved = resolvePath(config_, path_argument->second);
    if (!resolved.error.empty()) {
        return ToolResult::failed(resolved.error);
    }

    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(resolved.path, filesystem_error)) {
        return ToolResult::failed("path is not a readable regular file: " +
                                  resolved.path.string());
    }

    const auto size = std::filesystem::file_size(resolved.path, filesystem_error);
    if (filesystem_error) {
        return ToolResult::failed("could not inspect file size: " +
                                  filesystem_error.message());
    }
    if (size > config_.max_read_bytes) {
        return ToolResult::failed("file exceeds max_read_bytes limit of " +
                                  std::to_string(config_.max_read_bytes));
    }

    std::ifstream input(resolved.path, std::ios::binary);
    if (!input) {
        return ToolResult::failed("could not open file for reading: " +
                                  resolved.path.string());
    }

    std::ostringstream contents;
    contents << input.rdbuf();
    if (input.bad()) {
        return ToolResult::failed("error while reading file: " + resolved.path.string());
    }
    return ToolResult::ok(contents.str());
}

WriteFileTool::WriteFileTool(FileToolConfig config) : config_(std::move(config)) {}

std::string_view WriteFileTool::name() const noexcept {
    return "write_file";
}

std::string_view WriteFileTool::description() const noexcept {
    return "Write a file. Arguments: path, content, and optional append (true/false).";
}

ToolResult WriteFileTool::execute(const ToolArguments &arguments) {
    const auto path_argument = arguments.find("path");
    const auto content_argument = arguments.find("content");
    if (path_argument == arguments.end()) {
        return ToolResult::failed("write_file requires a 'path' argument");
    }
    if (content_argument == arguments.end()) {
        return ToolResult::failed("write_file requires a 'content' argument");
    }

    bool append = false;
    std::string append_error;
    if (!parseAppendFlag(arguments, append, append_error)) {
        return ToolResult::failed(std::move(append_error));
    }

    const auto resolved = resolvePath(config_, path_argument->second);
    if (!resolved.error.empty()) {
        return ToolResult::failed(resolved.error);
    }

    try {
        const auto parent = resolved.path.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            if (!config_.create_parent_directories) {
                return ToolResult::failed("parent directory does not exist: " +
                                          parent.string());
            }
            std::filesystem::create_directories(parent);
        }
    } catch (const std::filesystem::filesystem_error &error) {
        return ToolResult::failed(std::string("could not prepare parent directory: ") +
                                  error.what());
    }

    const auto mode = std::ios::binary | std::ios::out |
                      (append ? std::ios::app : std::ios::trunc);
    std::ofstream output(resolved.path, mode);
    if (!output) {
        return ToolResult::failed("could not open file for writing: " +
                                  resolved.path.string());
    }

    output.write(content_argument->second.data(),
                 static_cast<std::streamsize>(content_argument->second.size()));
    output.close();
    if (!output) {
        return ToolResult::failed("error while writing file: " + resolved.path.string());
    }

    const std::string verb = append ? "appended " : "wrote ";
    return ToolResult::ok(verb + std::to_string(content_argument->second.size()) +
                          " bytes to " + resolved.path.string());
}

} // namespace oop_agent::tools
