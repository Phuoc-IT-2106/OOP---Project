#include "client/embedding_client.h"
#include "client/ollama_embedding_client.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace {

using oop_agent::client::EmbeddingClient;
using oop_agent::client::HttpResponse;
using oop_agent::client::OllamaEmbeddingClient;
using oop_agent::client::OllamaEmbeddingConfig;
using Json = nlohmann::json;

void expect(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testEmbeddingInterfaceIsAbstract() {
    static_assert(std::is_abstract_v<EmbeddingClient>);
}

void testOllamaEmbeddingClientWithFakeTransport() {
    OllamaEmbeddingConfig config;
    config.base_url = "http://ollama.test:11434/";
    config.endpoint = "/api/embed";
    config.model_name = "nomic-embed-text";
    config.timeout_seconds = 17;

    std::string captured_url;
    std::string captured_payload;
    long captured_timeout = 0;

    // This lambda replaces the real libcurl transport. The unit test therefore
    // verifies request building and response parsing without an Ollama process.
    const OllamaEmbeddingClient::HttpTransport fake_transport =
        [&](const std::string &url,
            const std::string &payload,
            long timeout_seconds) {
            captured_url = url;
            captured_payload = payload;
            captured_timeout = timeout_seconds;
            return HttpResponse{
                200,
                R"({"model":"nomic-embed-text","embeddings":[[0.25,-0.5,1.0]],"prompt_eval_count":4})",
                {}};
        };

    OllamaEmbeddingClient client(config, fake_transport);
    const auto response = client.embed("persistent memory");

    expect(response.success, "fake Ollama embedding request should succeed");
    expect(captured_url == "http://ollama.test:11434/api/embed",
           "client should join base URL and endpoint without duplicate slash");
    expect(captured_timeout == 17,
           "client should forward configured timeout to the transport");

    const auto payload = Json::parse(captured_payload);
    expect(payload["model"] == "nomic-embed-text" &&
               payload["input"] == "persistent memory",
           "client should send model and input fields expected by /api/embed");

    expect(response.embedding.size() == 3 &&
               std::abs(response.embedding[0] - 0.25) < 1e-12 &&
               std::abs(response.embedding[1] + 0.5) < 1e-12 &&
               std::abs(response.embedding[2] - 1.0) < 1e-12,
           "client should parse embeddings[0] into std::vector<double>");
    expect(response.prompt_tokens == 4,
           "client should parse Ollama prompt token metadata");
}

void testOllamaEmbeddingClientRejectsInvalidResponses() {
    OllamaEmbeddingClient missing_vector(
        {},
        [](const std::string &, const std::string &, long) {
            return HttpResponse{200, R"({"embeddings":[]})", {}};
        });
    expect(!missing_vector.embed("text").success,
           "client should reject an empty embeddings array");

    OllamaEmbeddingClient invalid_coordinate(
        {},
        [](const std::string &, const std::string &, long) {
            return HttpResponse{200, R"({"embeddings":[[0.1,"bad"]]})", {}};
        });
    expect(!invalid_coordinate.embed("text").success,
           "client should reject non-numeric vector coordinates");

    OllamaEmbeddingClient server_error(
        {},
        [](const std::string &, const std::string &, long) {
            return HttpResponse{500, R"({"error":"model unavailable"})", {}};
        });
    const auto failure = server_error.embed("text");
    expect(!failure.success &&
               failure.error_message.find("HTTP 500") != std::string::npos,
           "client should preserve non-success HTTP status");
}

void testOllamaEmbeddingClientValidatesInputAndConfig() {
    OllamaEmbeddingClient client(
        {},
        [](const std::string &, const std::string &, long) {
            return HttpResponse{200, R"({"embeddings":[[1.0]]})", {}};
        });
    expect(!client.embed("   ").success,
           "client should reject blank embedding input");

    OllamaEmbeddingConfig invalid_config;
    invalid_config.timeout_seconds = 0;
    OllamaEmbeddingClient invalid_timeout(
        invalid_config,
        [](const std::string &, const std::string &, long) {
            return HttpResponse{};
        });
    expect(!invalid_timeout.embed("text").success,
           "client should reject a non-positive timeout");
}

} // namespace

int main() {
    try {
        testEmbeddingInterfaceIsAbstract();
        testOllamaEmbeddingClientWithFakeTransport();
        testOllamaEmbeddingClientRejectsInvalidResponses();
        testOllamaEmbeddingClientValidatesInputAndConfig();
        std::cout << "All client tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
