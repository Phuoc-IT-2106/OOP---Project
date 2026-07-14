#include "memory_tool.h"

#include <exception>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace oop_agent::tools {
namespace {

bool validateConfig(const MemoryToolConfig &config, std::string &error) {
    if (config.default_search_limit == 0 || config.max_search_limit == 0 ||
        config.default_search_limit > config.max_search_limit) {
        error = "invalid memory search limits";
        return false;
    }
    if (config.max_content_bytes == 0 || config.max_tags_bytes == 0 ||
        config.max_query_bytes == 0) {
        error = "memory byte limits must be positive";
        return false;
    }
    return true;
}

bool parseSearchLimit(const ToolArguments &arguments,
                      const MemoryToolConfig &config,
                      std::size_t &limit,
                      std::string &error) {
    const auto argument = arguments.find("limit");
    if (argument == arguments.end() || argument->second.empty()) {
        limit = config.default_search_limit;
        return true;
    }

    try {
        std::size_t parsed_characters = 0;
        const auto parsed = std::stoull(argument->second, &parsed_characters);
        if (parsed_characters != argument->second.size() || parsed == 0 ||
            parsed > config.max_search_limit ||
            parsed > std::numeric_limits<std::size_t>::max()) {
            throw std::out_of_range("memory search limit is outside allowed range");
        }
        limit = static_cast<std::size_t>(parsed);
        return true;
    } catch (const std::exception &) {
        error = "'limit' must be an integer from 1 to " +
                std::to_string(config.max_search_limit);
        return false;
    }
}

std::string formatEntries(const std::vector<MemoryEntry> &entries) {
    if (entries.empty()) {
        return "No matching memories found.";
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto &entry = entries[index];
        output << index + 1 << ". [memory_id=" << entry.id << "] "
               << entry.content << '\n';
        if (!entry.tags.empty()) {
            output << "Tags: " << entry.tags << '\n';
        }
        if (!entry.created_at.empty()) {
            output << "Created: " << entry.created_at << '\n';
        }
        if (index + 1 != entries.size()) {
            output << '\n';
        }
    }
    return output.str();
}

} // namespace

MemorySaveTool::MemorySaveTool(std::shared_ptr<MemoryStore> store,
                               MemoryToolConfig config)
    : store_(std::move(store)), config_(std::move(config)) {}

std::string_view MemorySaveTool::name() const noexcept {
    return "memory_save";
}

std::string_view MemorySaveTool::description() const noexcept {
    return "Save persistent memory. Arguments: content and optional comma-separated tags.";
}

ToolResult MemorySaveTool::execute(const ToolArguments &arguments) {
    const auto content_argument = arguments.find("content");
    if (content_argument == arguments.end() || content_argument->second.empty()) {
        return ToolResult::failed("memory_save requires non-empty 'content'");
    }
    std::string config_error;
    if (!validateConfig(config_, config_error)) {
        return ToolResult::failed(std::move(config_error));
    }
    if (!store_) {
        return ToolResult::failed("memory store is not configured");
    }
    if (content_argument->second.size() > config_.max_content_bytes) {
        return ToolResult::failed("memory content exceeds max_content_bytes limit of " +
                                  std::to_string(config_.max_content_bytes));
    }

    const auto tags_argument = arguments.find("tags");
    const std::string tags =
        tags_argument == arguments.end() ? std::string{} : tags_argument->second;
    if (tags.size() > config_.max_tags_bytes) {
        return ToolResult::failed("memory tags exceed max_tags_bytes limit of " +
                                  std::to_string(config_.max_tags_bytes));
    }

    try {
        const auto response = store_->save(content_argument->second, tags);
        if (!response.success) {
            return ToolResult::failed(response.error_message.empty()
                                          ? "memory store could not save the entry"
                                          : response.error_message);
        }
        return ToolResult::ok("Saved memory with id " + std::to_string(response.id) + ".");
    } catch (const std::exception &error) {
        return ToolResult::failed(std::string("memory save failed: ") + error.what());
    } catch (...) {
        return ToolResult::failed("memory save failed with an unknown error");
    }
}

MemorySearchTool::MemorySearchTool(std::shared_ptr<MemoryStore> store,
                                   MemoryToolConfig config)
    : store_(std::move(store)), config_(std::move(config)) {}

std::string_view MemorySearchTool::name() const noexcept {
    return "memory_search";
}

std::string_view MemorySearchTool::description() const noexcept {
    return "Search persistent memory by content or tags. Arguments: query and optional limit.";
}

ToolResult MemorySearchTool::execute(const ToolArguments &arguments) {
    const auto query_argument = arguments.find("query");
    if (query_argument == arguments.end() || query_argument->second.empty()) {
        return ToolResult::failed("memory_search requires non-empty 'query'");
    }
    std::string config_error;
    if (!validateConfig(config_, config_error)) {
        return ToolResult::failed(std::move(config_error));
    }
    if (!store_) {
        return ToolResult::failed("memory store is not configured");
    }
    if (query_argument->second.size() > config_.max_query_bytes) {
        return ToolResult::failed("memory query exceeds max_query_bytes limit of " +
                                  std::to_string(config_.max_query_bytes));
    }

    std::size_t limit = 0;
    std::string limit_error;
    if (!parseSearchLimit(arguments, config_, limit, limit_error)) {
        return ToolResult::failed(std::move(limit_error));
    }

    try {
        const auto response = store_->search(query_argument->second, limit);
        if (!response.success) {
            return ToolResult::failed(response.error_message.empty()
                                          ? "memory store could not search entries"
                                          : response.error_message);
        }
        return ToolResult::ok(formatEntries(response.entries));
    } catch (const std::exception &error) {
        return ToolResult::failed(std::string("memory search failed: ") + error.what());
    } catch (...) {
        return ToolResult::failed("memory search failed with an unknown error");
    }
}

} // namespace oop_agent::tools
