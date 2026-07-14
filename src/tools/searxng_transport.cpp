#include "web_search_tool.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace oop_agent::tools {
namespace {

using Json = nlohmann::json;

std::size_t writeCallback(char *contents,
                          std::size_t size,
                          std::size_t count,
                          void *user_data) {
    auto *body = static_cast<std::string *>(user_data);
    if (contents == nullptr || body == nullptr ||
        (size != 0 && count > std::numeric_limits<std::size_t>::max() / size)) {
        return 0;
    }
    const auto byte_count = size * count;
    body->append(contents, byte_count);
    return byte_count;
}

std::string joinUrl(const std::string &base_url, const std::string &endpoint) {
    const bool base_has_slash = !base_url.empty() && base_url.back() == '/';
    const bool endpoint_has_slash = !endpoint.empty() && endpoint.front() == '/';
    if (base_has_slash && endpoint_has_slash) {
        return base_url + endpoint.substr(1);
    }
    if (!base_has_slash && !endpoint_has_slash) {
        return base_url + "/" + endpoint;
    }
    return base_url + endpoint;
}

std::string percentEncode(const std::string &value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (const unsigned char character : value) {
        if (std::isalnum(character) || character == '-' || character == '_' ||
            character == '.' || character == '~') {
            encoded << static_cast<char>(character);
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0')
                    << static_cast<int>(character);
        }
    }
    return encoded.str();
}

std::string buildSearchUrl(const WebSearchConfig &config,
                           const WebSearchRequest &request) {
    std::ostringstream url;
    url << joinUrl(config.base_url, config.endpoint)
        << "?q=" << percentEncode(request.query)
        << "&format=json"
        << "&safesearch=" << request.safe_search;
    if (!request.language.empty()) {
        url << "&language=" << percentEncode(request.language);
    }
    if (!request.categories.empty()) {
        url << "&categories=" << percentEncode(request.categories);
    }
    return url.str();
}

WebSearchResponse parseResponse(long status_code,
                                const std::string &body,
                                std::size_t max_results) {
    WebSearchResponse response;
    response.status_code = status_code;
    if (status_code < 200 || status_code >= 300) {
        return response;
    }

    try {
        const auto json = Json::parse(body);
        if (!json.contains("results") || !json["results"].is_array()) {
            response.error_message = "SearXNG response is missing the results array";
            return response;
        }

        for (const auto &item : json["results"]) {
            if (response.results.size() >= max_results) {
                break;
            }
            if (!item.is_object() || !item.contains("title") ||
                !item["title"].is_string() || !item.contains("url") ||
                !item["url"].is_string()) {
                continue;
            }

            WebSearchResult result;
            result.title = item["title"].get<std::string>();
            result.url = item["url"].get<std::string>();
            if (item.contains("content") && item["content"].is_string()) {
                result.snippet = item["content"].get<std::string>();
            }
            response.results.push_back(std::move(result));
        }
    } catch (const Json::parse_error &) {
        response.error_message = "SearXNG returned malformed JSON";
    } catch (const Json::exception &error) {
        response.error_message = std::string("invalid SearXNG response: ") + error.what();
    }
    return response;
}

WebSearchResponse performSearch(const WebSearchConfig &config,
                                const WebSearchRequest &request) {
    WebSearchResponse response;

    static const bool curl_initialized = [] {
        return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    }();
    if (!curl_initialized) {
        response.error_message = "failed to initialize curl for web search";
        return response;
    }

    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        response.error_message = "failed to initialize curl for web search";
        return response;
    }

    const std::string url = buildSearchUrl(config, request);
    std::string body;
    char error_buffer[CURL_ERROR_SIZE] = {};

    const auto setOption = [&](CURLoption option, auto value) {
        return curl_easy_setopt(curl, option, value) == CURLE_OK;
    };
    const bool configured =
        setOption(CURLOPT_URL, url.c_str()) &&
        setOption(CURLOPT_WRITEFUNCTION, writeCallback) &&
        setOption(CURLOPT_WRITEDATA, &body) &&
        setOption(CURLOPT_ERRORBUFFER, error_buffer) &&
        setOption(CURLOPT_TIMEOUT, config.timeout_seconds) &&
        setOption(CURLOPT_CONNECTTIMEOUT, config.timeout_seconds) &&
        setOption(CURLOPT_FOLLOWLOCATION, 1L) &&
        setOption(CURLOPT_NOSIGNAL, 1L) &&
        setOption(CURLOPT_USERAGENT, "oop-agent-framework/1.0");
    if (!configured) {
        response.error_message = "failed to configure curl for web search";
        curl_easy_cleanup(curl);
        return response;
    }

    const CURLcode result = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        if (result == CURLE_OPERATION_TIMEDOUT) {
            response.error_message = "SearXNG request timed out";
        } else {
            response.error_message = error_buffer[0] != '\0'
                                         ? error_buffer
                                         : curl_easy_strerror(result);
        }
        response.status_code = status_code;
        return response;
    }
    return parseResponse(status_code, body, request.max_results);
}

} // namespace

WebSearchTool::WebSearchTool(WebSearchConfig config)
    : WebSearchTool(config, makeDefaultSearxngTransport(config)) {}

WebSearchTool::SearchTransport makeDefaultSearxngTransport(WebSearchConfig config) {
    return [config = std::move(config)](const WebSearchRequest &request) {
        return performSearch(config, request);
    };
}

} // namespace oop_agent::tools
