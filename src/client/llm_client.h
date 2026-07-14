#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace oop_agent::client {

struct ChatMessage {
    std::string role;
    std::string content;
};

struct ChatRequest {
    std::vector<ChatMessage> messages;
};

struct ChatResponse {
    bool success{false};
    std::string content;
    std::string raw_json;
    std::string error_message;
    long latency_ms{0};
    std::int64_t prompt_tokens{0};
    std::int64_t completion_tokens{0};
};

class LLMClient {
  public:
    virtual ~LLMClient() = default;

    virtual ChatResponse chat(const ChatRequest &request) = 0;
};

} // namespace oop_agent::client
