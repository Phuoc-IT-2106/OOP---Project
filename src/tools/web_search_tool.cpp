#include "web_search_tool.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace oop_agent::tools {
namespace {

bool parseMaxResults(const ToolArguments &arguments,
                     const WebSearchConfig &config,
                     std::size_t &max_results,
                     std::string &error) {
    const auto argument = arguments.find("max_results");
    if (argument == arguments.end() || argument->second.empty()) {
        max_results = config.default_max_results;
        return true;
    }

    try {
        std::size_t parsed_characters = 0;
        const auto parsed = std::stoull(argument->second, &parsed_characters);
        if (parsed_characters != argument->second.size() || parsed == 0 ||
            parsed > config.max_results_limit ||
            parsed > std::numeric_limits<std::size_t>::max()) {
            throw std::out_of_range("max_results is outside the allowed range");
        }
        max_results = static_cast<std::size_t>(parsed);
        return true;
    } catch (const std::exception &) {
        error = "'max_results' must be an integer from 1 to " +
                std::to_string(config.max_results_limit);
        return false;
    }
}

std::string formatResults(const std::vector<WebSearchResult> &results,
                          std::size_t max_results) {
    if (results.empty()) {
        return "No search results found.";
    }

    std::ostringstream output;
    const auto result_count = std::min(results.size(), max_results);
    for (std::size_t index = 0; index < result_count; ++index) {
        const auto &result = results[index];
        output << index + 1 << ". " << result.title << '\n'
               << "URL: " << result.url << '\n';
        if (!result.snippet.empty()) {
            output << "Snippet: " << result.snippet << '\n';
        }
        if (index + 1 != result_count) {
            output << '\n';
        }
    }
    return output.str();
}

} // namespace

WebSearchTool::WebSearchTool(WebSearchConfig config, SearchTransport transport)
    : config_(std::move(config)), transport_(std::move(transport)) {}

std::string_view WebSearchTool::name() const noexcept {
    return "web_search";
}

std::string_view WebSearchTool::description() const noexcept {
    return "Search the web through SearXNG. Arguments: query and optional max_results.";
}

ToolResult WebSearchTool::execute(const ToolArguments &arguments) {
    const auto query_argument = arguments.find("query");
    if (query_argument == arguments.end() || query_argument->second.empty()) {
        return ToolResult::failed("web_search requires a non-empty 'query' argument");
    }
    if (config_.base_url.empty() || config_.endpoint.empty()) {
        return ToolResult::failed("SearXNG base_url and endpoint must not be empty");
    }
    if (config_.timeout_seconds <= 0) {
        return ToolResult::failed("SearXNG timeout_seconds must be positive");
    }
    if (config_.safe_search < 0 || config_.safe_search > 2) {
        return ToolResult::failed("SearXNG safe_search must be 0, 1, or 2");
    }
    if (config_.default_max_results == 0 || config_.max_results_limit == 0 ||
        config_.default_max_results > config_.max_results_limit) {
        return ToolResult::failed("invalid web search result limits");
    }
    if (!transport_) {
        return ToolResult::failed("web search transport is not configured");
    }

    std::size_t max_results = 0;
    std::string max_results_error;
    if (!parseMaxResults(arguments, config_, max_results, max_results_error)) {
        return ToolResult::failed(std::move(max_results_error));
    }

    const WebSearchRequest request{query_argument->second,
                                   config_.language,
                                   config_.categories,
                                   config_.safe_search,
                                   max_results};
    try {
        const auto response = transport_(request);
        if (!response.error_message.empty()) {
            return ToolResult::failed(response.error_message);
        }
        if (response.status_code < 200 || response.status_code >= 300) {
            return ToolResult::failed("SearXNG returned HTTP " +
                                      std::to_string(response.status_code));
        }
        return ToolResult::ok(formatResults(response.results, max_results));
    } catch (const std::exception &error) {
        return ToolResult::failed(std::string("web search failed: ") + error.what());
    } catch (...) {
        return ToolResult::failed("web search failed with an unknown error");
    }
}

} // namespace oop_agent::tools
