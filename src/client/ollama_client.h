#pragma once

#include "llm_client.h"
#include "ollama_http_transport.h"

#include <string>

namespace oop_agent::client {

struct OllamaConfig {
    std::string base_url{"http://localhost:11434"};
    std::string endpoint{"/api/chat"};
    std::string model_name{"qwen2.5:7b"};

    double temperature{0.2};
    int max_tokens{1024};
    long timeout_seconds{60};
};

class OllamaClient final : public LLMClient {
  public:
    // Giữ alias này để các test/code cũ vẫn inject transport được.
    using HttpTransport = OllamaHttpTransport;

    explicit OllamaClient(
        OllamaConfig config
    );

    OllamaClient(
        OllamaConfig config,
        HttpTransport transport
    );

    // Hỗ trợ cả text-only và multimodal.
    ChatResponse chat(
        const ChatRequest &request
    ) override;

    const OllamaConfig &config() const;

  private:
    OllamaConfig config_;
    HttpTransport transport_;

    std::string buildUrl() const;

    // Week 8:
    // Nếu ChatMessage.images không rỗng,
    // payload sẽ thêm trường "images".
    std::string buildPayload(
        const ChatRequest &request
    ) const;

    ChatResponse parseResponse(
        const HttpResponse &http_response,
        long latency_ms
    ) const;
};

} // namespace oop_agent::client
