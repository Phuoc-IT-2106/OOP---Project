#include "client/base64.h"
#include "client/ollama_client.h"

#include <iostream>

int main() {
    using namespace oop_agent::client;

    OllamaConfig config;
    config.base_url = "http://localhost:11434";
    config.endpoint = "/api/chat";
    config.model_name = "qwen2.5vl:7b";

    OllamaClient client(config);

    const std::string image_base64 =
        Base64::encodeFile(
            "output/screenshots/gui_test.png"
        );

    ChatMessage message;
    message.role = "user";
    message.content =
        "Analyze this screenshot and return exactly one GUI action "
        "as JSON. Allowed actions: click, type_text, key_press, done.";

    message.images.push_back(image_base64);

    ChatRequest request;
    request.messages.push_back(message);

    const ChatResponse response =
        client.chat(request);

    if (!response.success) {
        std::cerr
            << "VLM request failed: "
            << response.error_message
            << '\n';

        return 1;
    }

    std::cout
        << "VLM response:\n"
        << response.content
        << '\n';

    return 0;
}