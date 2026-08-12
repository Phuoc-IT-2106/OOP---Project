#include "client/base64.h"
#include "client/ollama_client.h"
#include "client/embedding_client.h"
#include "client/ollama_embedding_client.h"

#include <cmath>
#include <filesystem>
#include <fstream>
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
void testBase64FileEncoding() {
    const std::filesystem::path test_file =
        "test_base64.bin";

    {
        std::ofstream output(
            test_file,
            std::ios::binary
        );

        output << "Man";
    }

    const std::string encoded =
        oop_agent::client::Base64::encodeFile(
            test_file
        );

    expect(
        encoded == "TWFu",
        "Base64 encoder should encode 'Man' as TWFu"
    );

    std::filesystem::remove(test_file);
}

void testOllamaClientBuildsMultimodalPayload() {
    using oop_agent::client::ChatMessage;
    using oop_agent::client::ChatRequest;
    using oop_agent::client::OllamaClient;
    using oop_agent::client::OllamaConfig;

    OllamaConfig config;
    config.base_url = "http://ollama.test:11434";
    config.endpoint = "/api/chat";
    config.model_name = "test-vlm";

    std::string captured_payload;

    const OllamaClient::HttpTransport fake_transport =
        [&](const std::string &,
            const std::string &payload,
            long) {
            captured_payload = payload;

            return HttpResponse{
                200,
                R"({
                    "message": {
                        "role": "assistant",
                        "content": "{\"action\":\"click\",\"x\":\"100\",\"y\":\"200\"}"
                    },
                    "prompt_eval_count": 10,
                    "eval_count": 5
                })",
                {}
            };
        };

    OllamaClient client(
        config,
        fake_transport
    );

    ChatMessage message;
    message.role = "user";
    message.content =
        "Analyze this screenshot.";
    message.images.push_back("TWFu");

    ChatRequest request;
    request.messages.push_back(message);

    const auto response =
        client.chat(request);

    expect(
        response.success,
        "multimodal Ollama request should succeed"
    );

    const Json payload =
        Json::parse(captured_payload);

    expect(
        payload["messages"][0].contains("images"),
        "multimodal payload should contain images"
    );

    expect(
        payload["messages"][0]["images"][0] == "TWFu",
        "multimodal payload should include Base64 image"
    );
}
void testOllamaMultimodalActionResponse() {
    using oop_agent::client::ChatMessage;
    using oop_agent::client::ChatRequest;
    using oop_agent::client::OllamaClient;
    using oop_agent::client::OllamaConfig;

    OllamaConfig config;
    config.base_url = "http://ollama.test:11434";
    config.endpoint = "/api/chat";
    config.model_name = "test-vlm";

    std::string captured_payload;

    const OllamaClient::HttpTransport fake_transport =
        [&](const std::string &,
            const std::string &payload,
            long) {
            captured_payload = payload;

            return HttpResponse{
                200,
                R"({
                    "message": {
                        "role": "assistant",
                        "content": "{\"action\":\"click\",\"x\":500,\"y\":300,\"button\":1}"
                    },
                    "prompt_eval_count": 25,
                    "eval_count": 8
                })",
                {}
            };
        };

    OllamaClient client(
        config,
        fake_transport
    );

    ChatMessage message;
    message.role = "user";
    message.content =
        "Analyze the screenshot and return exactly one GUI action.";

    message.images.push_back(
        "iVBORw0KGgoAAAATESTBASE64"
    );

    ChatRequest request;
    request.messages.push_back(message);

    const auto response =
        client.chat(request);

    expect(
        response.success,
        "multimodal VLM request should succeed"
    );

    const Json payload =
        Json::parse(captured_payload);

    expect(
        payload["messages"][0].contains("images"),
        "multimodal request should contain images field"
    );

    expect(
        payload["messages"][0]["images"][0]
            == "iVBORw0KGgoAAAATESTBASE64",
        "multimodal request should preserve Base64 image"
    );

    const Json action =
        Json::parse(response.content);

    expect(
        action["action"] == "click",
        "VLM should return a GUI action"
    );

    expect(
        action["x"] == 500 &&
        action["y"] == 300 &&
        action["button"] == 1,
        "VLM click action should contain coordinates and button"
    );
}
int main() {
    try {
        testEmbeddingInterfaceIsAbstract();
        testOllamaEmbeddingClientWithFakeTransport();
        testOllamaEmbeddingClientRejectsInvalidResponses();
        testOllamaEmbeddingClientValidatesInputAndConfig();
        testBase64FileEncoding();
        testOllamaClientBuildsMultimodalPayload();
        testOllamaMultimodalActionResponse();
        std::cout << "All client tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
