#include "ollama_embedding_client.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace oop_agent::client {
namespace {

using Json = nlohmann::json;

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

EmbeddingResponse failure(std::string message,
                          std::string raw_json = {},
                          long latency_ms = 0) {
    EmbeddingResponse response;
    response.error_message = std::move(message);
    response.raw_json = std::move(raw_json);
    response.latency_ms = latency_ms;
    return response;
}

std::int64_t readPromptTokens(const Json &json) {
    if (!json.contains("prompt_eval_count")) {
        return 0;
    }

    const auto &value = json["prompt_eval_count"];
    if (value.is_number_integer()) {
        return value.get<std::int64_t>();
    }
    if (!value.is_number_unsigned()) {
        return 0;
    }

    const auto unsigned_value = value.get<std::uint64_t>();
    if (unsigned_value > static_cast<std::uint64_t>(
                             std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>(unsigned_value);
}

} // namespace

OllamaEmbeddingClient::OllamaEmbeddingClient(OllamaEmbeddingConfig config)
    : OllamaEmbeddingClient(std::move(config), defaultOllamaHttpTransport) {}

OllamaEmbeddingClient::OllamaEmbeddingClient(OllamaEmbeddingConfig config,
                                             HttpTransport transport)
    : config_(std::move(config)), transport_(std::move(transport)) {}

EmbeddingResponse OllamaEmbeddingClient::embed(const std::string &text) {
    const auto started_at = std::chrono::steady_clock::now();
    const auto elapsedMs = [&started_at] {
        const auto finished_at = std::chrono::steady_clock::now();
        return static_cast<long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                finished_at - started_at)
                .count());
    };

    if (isBlank(config_.base_url)) {
        return failure("Ollama embedding base_url is empty", {}, elapsedMs());
    }
    if (isBlank(config_.endpoint)) {
        return failure("Ollama embedding endpoint is empty", {}, elapsedMs());
    }
    if (isBlank(config_.model_name)) {
        return failure("Ollama embedding model_name is empty", {}, elapsedMs());
    }
    if (config_.timeout_seconds <= 0) {
        return failure("Ollama embedding timeout_seconds must be positive",
                       {},
                       elapsedMs());
    }
    if (isBlank(text)) {
        return failure("embedding text must not be empty", {}, elapsedMs());
    }
    if (!transport_) {
        return failure("HTTP transport is not configured", {}, elapsedMs());
    }

    try {
        const auto http_response =
            transport_(buildUrl(), buildPayload(text), config_.timeout_seconds);
        return parseResponse(http_response, elapsedMs());
    } catch (const std::exception &error) {
        return failure(std::string("OllamaEmbeddingClient error: ") + error.what(),
                       {},
                       elapsedMs());
    } catch (...) {
        return failure("OllamaEmbeddingClient error: unknown exception",
                       {},
                       elapsedMs());
    }
}

const OllamaEmbeddingConfig &OllamaEmbeddingClient::config() const {
    return config_;
}

std::string OllamaEmbeddingClient::buildUrl() const {
    return joinUrl(config_.base_url, config_.endpoint);
}

std::string OllamaEmbeddingClient::buildPayload(const std::string &text) const {
    // Ollama /api/embed accepts either one string or an array in "input".
    // This client exposes one-text-at-a-time embedding, so the response should
    // contain exactly one vector at embeddings[0].
    return Json{{"model", config_.model_name}, {"input", text}}.dump(
        -1, ' ', false, nlohmann::json::error_handler_t::replace);
}

EmbeddingResponse OllamaEmbeddingClient::parseResponse(
    const HttpResponse &http_response,
    long latency_ms) const {
    if (!http_response.error_message.empty()) {
        return failure(http_response.error_message,
                       http_response.body,
                       latency_ms);
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
        return failure("malformed JSON response",
                       http_response.body,
                       latency_ms);
    }

    // /api/embed returns number[][] even for one input. Validate every level
    // explicitly so a changed or malformed server response cannot leak into
    // cosine-similarity code as an empty/invalid vector.
    if (!parsed.contains("embeddings") || !parsed["embeddings"].is_array() ||
        parsed["embeddings"].empty() || !parsed["embeddings"][0].is_array() ||
        parsed["embeddings"][0].empty()) {
        return failure("missing embedding vector",
                       http_response.body,
                       latency_ms);
    }

    std::vector<double> embedding;
    embedding.reserve(parsed["embeddings"][0].size());
    for (const auto &value : parsed["embeddings"][0]) {
        if (!value.is_number()) {
            return failure("embedding vector contains a non-number",
                           http_response.body,
                           latency_ms);
        }

        const double coordinate = value.get<double>();
        if (!std::isfinite(coordinate)) {
            return failure("embedding vector contains a non-finite number",
                           http_response.body,
                           latency_ms);
        }
        embedding.push_back(coordinate);
    }

    EmbeddingResponse response;
    response.success = true;
    response.embedding = std::move(embedding);
    response.raw_json = http_response.body;
    response.latency_ms = latency_ms;
    response.prompt_tokens = readPromptTokens(parsed);
    return response;
}

} // namespace oop_agent::client
