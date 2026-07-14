#include "ollama_client.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <sstream>
#include <utility>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace oop_agent::client {
namespace {

using Json = nlohmann::json;

std::size_t writeCallback(char *contents, std::size_t size, std::size_t nmemb, void *user_data) {
    auto *buffer = static_cast<std::string *>(user_data);
    if (buffer == nullptr || contents == nullptr) {
        return 0;
    }
    if (size != 0 && nmemb > std::numeric_limits<std::size_t>::max() / size) {
        return 0;
    }
    const auto total_size = size * nmemb;
    buffer->append(contents, total_size);
    return total_size;
}

bool isBlank(const std::string &value) {
    for (const unsigned char character : value) {
        if (!std::isspace(character)) {
            return false;
        }
    }
    return true;
}

std::string joinUrl(const std::string &base_url, const std::string &endpoint) {
    if (base_url.empty()) {
        return endpoint;
    }
    if (endpoint.empty()) {
        return base_url;
    }

    const bool base_has_slash = base_url.back() == '/';
    const bool endpoint_has_slash = endpoint.front() == '/';
    if (base_has_slash && endpoint_has_slash) {
        return base_url + endpoint.substr(1);
    }
    if (!base_has_slash && !endpoint_has_slash) {
        return base_url + "/" + endpoint;
    }
    return base_url + endpoint;
}

ChatResponse failure(std::string message, std::string raw_json = {}, long latency_ms = 0) {
    ChatResponse response;
    response.success = false;
    response.error_message = std::move(message);
    response.raw_json = std::move(raw_json);
    response.latency_ms = latency_ms;
    return response;
}

std::int64_t readIntegerField(const Json &json, const char *field_name) {
    if (!json.contains(field_name)) {
        return 0;
    }

    const auto &value = json[field_name];
    if (value.is_number_integer()) {
        return value.get<std::int64_t>();
    }
    if (!value.is_number_unsigned()) {
        return 0;
    }

    const auto unsigned_value = value.get<std::uint64_t>();
    if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>(unsigned_value);
}

} // namespace

OllamaClient::OllamaClient(OllamaConfig config)
    : OllamaClient(std::move(config), defaultOllamaHttpTransport) {}

OllamaClient::OllamaClient(OllamaConfig config, HttpTransport transport)
    : config_(std::move(config)), transport_(std::move(transport)) {}

ChatResponse OllamaClient::chat(const ChatRequest &request) {
    const auto started_at = std::chrono::steady_clock::now();

    const auto elapsed_ms = [&started_at] {
        const auto finished_at = std::chrono::steady_clock::now();
        return static_cast<long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(finished_at - started_at)
                .count());
    };

    if (isBlank(config_.base_url)) {
        return failure("Ollama base_url is empty", {}, elapsed_ms());
    }
    if (isBlank(config_.endpoint)) {
        return failure("Ollama endpoint is empty", {}, elapsed_ms());
    }
    if (isBlank(config_.model_name)) {
        return failure("Ollama model_name is empty", {}, elapsed_ms());
    }
    if (!std::isfinite(config_.temperature) || config_.temperature < 0.0) {
        return failure("Ollama temperature must be a non-negative finite number", {}, elapsed_ms());
    }
    if (config_.max_tokens <= 0) {
        return failure("Ollama max_tokens must be positive", {}, elapsed_ms());
    }
    if (config_.timeout_seconds <= 0) {
        return failure("Ollama timeout_seconds must be positive", {}, elapsed_ms());
    }
    if (request.messages.empty()) {
        return failure("chat request must contain at least one message", {}, elapsed_ms());
    }
    for (const auto &message : request.messages) {
        if (isBlank(message.role)) {
            return failure("chat message role must not be empty", {}, elapsed_ms());
        }
    }
    if (!transport_) {
        return failure("HTTP transport is not configured", {}, elapsed_ms());
    }

    try {
        const auto payload = buildPayload(request);
        const auto http_response = transport_(buildUrl(), payload, config_.timeout_seconds);
        return parseResponse(http_response, elapsed_ms());
    } catch (const std::exception &error) {
        return failure(std::string("OllamaClient error: ") + error.what(), {}, elapsed_ms());
    } catch (...) {
        return failure("OllamaClient error: unknown exception", {}, elapsed_ms());
    }
}

const OllamaConfig &OllamaClient::config() const {
    return config_;
}

std::string OllamaClient::buildUrl() const {
    return joinUrl(config_.base_url, config_.endpoint);
}

std::string OllamaClient::buildPayload(const ChatRequest &request) const {
    Json messages = Json::array();
    for (const auto &message : request.messages) {
        messages.push_back({{"role", message.role}, {"content", message.content}});
    }

    Json payload{{"model", config_.model_name},
                 {"messages", messages},
                 {"stream", false},
                 {"options",
                  {{"temperature", config_.temperature}, {"num_predict", config_.max_tokens}}}};

    return payload.dump();
}

ChatResponse OllamaClient::parseResponse(const HttpResponse &http_response, long latency_ms) const {
    if (!http_response.error_message.empty()) {
        return failure(http_response.error_message, http_response.body, latency_ms);
    }

    if (http_response.status_code < 200 || http_response.status_code >= 300) {
        std::ostringstream error;
        error << "HTTP " << http_response.status_code;
        if (!http_response.body.empty()) {
            error << ": " << http_response.body;
        }
        return failure(error.str(), http_response.body, latency_ms);
    }

    if (http_response.body.empty()) {
        return failure("empty response", {}, latency_ms);
    }

    Json parsed;
    try {
        parsed = Json::parse(http_response.body);
    } catch (const Json::parse_error &) {
        return failure("malformed JSON response", http_response.body, latency_ms);
    }

    if (!parsed.contains("message") || !parsed["message"].is_object() ||
        !parsed["message"].contains("content") || !parsed["message"]["content"].is_string()) {
        return failure("missing assistant content", http_response.body, latency_ms);
    }

    ChatResponse response;
    response.success = true;
    response.content = parsed["message"]["content"].get<std::string>();
    response.raw_json = http_response.body;
    response.latency_ms = latency_ms;
    response.prompt_tokens = readIntegerField(parsed, "prompt_eval_count");
    response.completion_tokens = readIntegerField(parsed, "eval_count");
    return response;
}

HttpResponse defaultOllamaHttpTransport(const std::string &url,
                                        const std::string &payload,
                                        long timeout_seconds) {
    HttpResponse response;

    static const bool curl_initialized = [] {
        return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    }();
    if (!curl_initialized) {
        response.error_message = "failed to initialize curl";
        return response;
    }

    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        response.error_message = "failed to initialize curl";
        return response;
    }

    const auto max_curl_payload_size =
        static_cast<std::uint64_t>(std::numeric_limits<curl_off_t>::max());
    if (static_cast<std::uint64_t>(payload.size()) > max_curl_payload_size) {
        response.error_message = "request payload is too large";
        curl_easy_cleanup(curl);
        return response;
    }

    char error_buffer[CURL_ERROR_SIZE] = {};
    std::string response_body;
    curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (headers == nullptr) {
        response.error_message = "failed to configure HTTP headers";
        curl_easy_cleanup(curl);
        return response;
    }

    const auto failSetup = [&](const char *option_name, CURLcode code) {
        std::ostringstream error;
        error << "failed to configure curl option " << option_name << ": "
              << curl_easy_strerror(code);
        response.error_message = error.str();
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return response;
    };

#define OOP_AGENT_SET_CURL_OPTION(option, value)                       \
    do {                                                               \
        const CURLcode option_result = curl_easy_setopt(curl, option, value); \
        if (option_result != CURLE_OK) {                               \
            return failSetup(#option, option_result);                  \
        }                                                              \
    } while (false)

    OOP_AGENT_SET_CURL_OPTION(CURLOPT_URL, url.c_str());
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_POST, 1L);
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_HTTPHEADER, headers);
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_ERRORBUFFER, error_buffer);
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_POSTFIELDS, payload.c_str());
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_POSTFIELDSIZE_LARGE,
                              static_cast<curl_off_t>(payload.size()));
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_WRITEFUNCTION, writeCallback);
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_WRITEDATA, &response_body);
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_TIMEOUT, timeout_seconds);
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_CONNECTTIMEOUT, timeout_seconds);
    OOP_AGENT_SET_CURL_OPTION(CURLOPT_NOSIGNAL, 1L);

#undef OOP_AGENT_SET_CURL_OPTION

    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        if (result == CURLE_OPERATION_TIMEDOUT) {
            response.error_message = "request timed out";
        } else {
            response.error_message =
                error_buffer[0] != '\0' ? error_buffer : curl_easy_strerror(result);
        }
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    response.body = std::move(response_body);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

} // namespace oop_agent::client
