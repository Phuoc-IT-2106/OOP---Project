#pragma once

#include "tool.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace oop_agent::tools {

struct WebSearchConfig {
    std::string base_url{"http://localhost:8080"};
    std::string endpoint{"/search"};
    std::string language{"all"};
    std::string categories{"general"};
    int safe_search{1};
    long timeout_seconds{15};
    std::size_t default_max_results{5};
    std::size_t max_results_limit{10};
};

struct WebSearchRequest {
    std::string query;
    std::string language;
    std::string categories;
    int safe_search{1};
    std::size_t max_results{5};
};

struct WebSearchResult {
    std::string title;
    std::string url;
    std::string snippet;
};

struct WebSearchResponse {
    long status_code{0};
    std::vector<WebSearchResult> results;
    std::string error_message;
};

class WebSearchTool final : public Tool {
  public:
    // Production constructor: creates a libcurl transport for the configured
    // SearXNG instance.
    explicit WebSearchTool(WebSearchConfig config = {});

    // Transport injection keeps HTTP outside the Tool logic and lets unit tests
    // run deterministically without Internet access.
    using SearchTransport = std::function<WebSearchResponse(const WebSearchRequest &)>;
    WebSearchTool(WebSearchConfig config, SearchTransport transport);

    std::string_view name() const noexcept override;
    std::string_view description() const noexcept override;
    ToolResult execute(const ToolArguments &arguments) override;

  private:
    WebSearchConfig config_;
    SearchTransport transport_;
};

// Kept public so another composition root can reuse or decorate the default
// SearXNG transport (for example, to add caching or retry behavior).
WebSearchTool::SearchTransport makeDefaultSearxngTransport(WebSearchConfig config);

} // namespace oop_agent::tools
