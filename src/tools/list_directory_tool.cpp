#include "list_directory_tool.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace oop_agent::tools {
namespace {

bool isWithin(const std::filesystem::path &root,
              const std::filesystem::path &candidate) {
    auto root_part = root.begin();
    auto candidate_part = candidate.begin();
    for (; root_part != root.end(); ++root_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *root_part != *candidate_part) {
            return false;
        }
    }
    return true;
}

std::size_t parseLimit(const ToolArguments &arguments,
                       const ListDirectoryConfig &config) {
    const auto found = arguments.find("max_entries");
    if (found == arguments.end() || found->second.empty()) {
        return config.default_max_entries;
    }
    std::size_t parsed_characters = 0;
    const auto parsed = std::stoull(found->second, &parsed_characters);
    if (parsed_characters != found->second.size() || parsed == 0 ||
        parsed > config.max_entries_limit) {
        throw std::invalid_argument(
            "'max_entries' must be from 1 to " +
            std::to_string(config.max_entries_limit));
    }
    return static_cast<std::size_t>(parsed);
}

} // namespace

ListDirectoryTool::ListDirectoryTool(ListDirectoryConfig config)
    : config_(std::move(config)) {}

std::string_view ListDirectoryTool::name() const noexcept {
    return "list_directory";
}

std::string_view ListDirectoryTool::description() const noexcept {
    return "List files and directories inside the benchmark workspace. "
           "Arguments: optional path and max_entries.";
}

ToolResult ListDirectoryTool::execute(const ToolArguments &arguments) {
    try {
        if (config_.default_max_entries == 0 ||
            config_.default_max_entries > config_.max_entries_limit) {
            return ToolResult::failed("invalid list_directory limits");
        }
        const auto root =
            std::filesystem::weakly_canonical(
                std::filesystem::absolute(config_.root_directory));
        const auto path_argument = arguments.find("path");
        const auto requested =
            path_argument == arguments.end() || path_argument->second.empty()
                ? std::filesystem::path{"."}
                : std::filesystem::path{path_argument->second};
        const auto target =
            std::filesystem::weakly_canonical(root / requested);
        if (!isWithin(root, target)) {
            return ToolResult::failed(
                "list_directory path escapes the workspace root");
        }
        if (!std::filesystem::is_directory(target)) {
            return ToolResult::failed("directory does not exist: " +
                                      requested.string());
        }

        const std::size_t limit = parseLimit(arguments, config_);
        std::vector<std::filesystem::directory_entry> entries;
        for (const auto &entry : std::filesystem::directory_iterator(target)) {
            entries.push_back(entry);
        }
        std::sort(entries.begin(),
                  entries.end(),
                  [](const auto &left, const auto &right) {
                      return left.path().filename().string() <
                             right.path().filename().string();
                  });

        std::ostringstream output;
        const std::size_t count = std::min(limit, entries.size());
        for (std::size_t index = 0; index < count; ++index) {
            const auto &entry = entries[index];
            output << (entry.is_directory() ? "[directory] " : "[file] ")
                   << entry.path().filename().string() << '\n';
        }
        if (entries.size() > limit) {
            output << "... " << entries.size() - limit
                   << " additional entries omitted\n";
        }
        return ToolResult::ok(output.str());
    } catch (const std::exception &error) {
        return ToolResult::failed(std::string("list_directory failed: ") +
                                  error.what());
    }
}

} // namespace oop_agent::tools
