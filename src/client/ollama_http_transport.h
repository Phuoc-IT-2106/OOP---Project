#pragma once

#include <functional>
#include <string>

namespace oop_agent::client {

struct HttpResponse {
    long status_code{0};
    std::string body;
    std::string error_message;
};

// Both chat and embedding clients use the same small HTTP boundary. Keeping it
// as a callable makes production code use libcurl while tests inject a lambda.
using OllamaHttpTransport =
    std::function<HttpResponse(const std::string &url,
                               const std::string &payload,
                               long timeout_seconds)>;

HttpResponse defaultOllamaHttpTransport(const std::string &url,
                                        const std::string &payload,
                                        long timeout_seconds);

} // namespace oop_agent::client
