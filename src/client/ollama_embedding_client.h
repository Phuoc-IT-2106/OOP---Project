#pragma once

#include "embedding_client.h"
#include "ollama_http_transport.h"

#include <string>

namespace oop_agent::client {

struct OllamaEmbeddingConfig {
    std::string base_url{"http://localhost:11434"};
    std::string endpoint{"/api/embed"};
    std::string model_name{"nomic-embed-text"};
    long timeout_seconds{30};
};

class OllamaEmbeddingClient final : public EmbeddingClient {
  public:
    // Reusing the common Ollama transport contract keeps HTTP details outside
    // embedding logic and lets tests inject a deterministic fake transport.
    using HttpTransport = OllamaHttpTransport;

    explicit OllamaEmbeddingClient(OllamaEmbeddingConfig config = {});
    OllamaEmbeddingClient(OllamaEmbeddingConfig config, HttpTransport transport);

    EmbeddingResponse embed(const std::string &text) override;

    const OllamaEmbeddingConfig &config() const;

  private:
    OllamaEmbeddingConfig config_;
    HttpTransport transport_;

    std::string buildUrl() const;
    std::string buildPayload(const std::string &text) const;
    EmbeddingResponse parseResponse(const HttpResponse &http_response,
                                    long latency_ms) const;
};

} // namespace oop_agent::client
