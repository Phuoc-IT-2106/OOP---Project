#pragma once

#include "llm_client.h"

#include <functional>
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

struct HttpResponse {
    long status_code{0};
    std::string body;
    std::string error_message;
};

class OllamaClient final : public LLMClient {
  public:
    using HttpTransport = std::function<HttpResponse(const std::string &url,
                                                     const std::string &payload,
                                                     long timeout_seconds)>;

    explicit OllamaClient(OllamaConfig config);
    OllamaClient(OllamaConfig config, HttpTransport transport);

    ChatResponse chat(const ChatRequest &request) override;

    const OllamaConfig &config() const;

  private:
    OllamaConfig config_;
    HttpTransport transport_;

    std::string buildUrl() const;
    std::string buildPayload(const ChatRequest &request) const;
    ChatResponse parseResponse(const HttpResponse &http_response, long latency_ms) const;
};

HttpResponse defaultOllamaHttpTransport(const std::string &url,
                                        const std::string &payload,
                                        long timeout_seconds);

} // namespace oop_agent::client
