#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace oop_agent::client {

struct EmbeddingResponse {
    bool success{false};
    std::vector<double> embedding;
    std::string raw_json;
    std::string error_message;
    long latency_ms{0};
    std::int64_t prompt_tokens{0};
};

class EmbeddingClient {
  public:
    // A virtual destructor lets callers own concrete clients through
    // std::unique_ptr<EmbeddingClient> or std::shared_ptr<EmbeddingClient>.
    virtual ~EmbeddingClient() = default;

    // Persistent Memory depends only on this small interface. It therefore
    // does not need to know whether vectors come from Ollama or another API.
    virtual EmbeddingResponse embed(const std::string &text) = 0;
};

} // namespace oop_agent::client
