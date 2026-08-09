#pragma once

#include "agent_loop.h"

#include <optional>
#include <string>

namespace oop_agent::agent {

struct AgentMessage {
    std::string sender_id;
    std::string recipient_id;
    bool success{false};
    std::string content;
};

struct ParallelAgentTask {
    std::string first_subtask;
    std::string second_subtask;
};

struct AgentWorkerResult {
    std::string agent_id;
    AgentRunResult run_result;
    std::optional<AgentMessage> peer_message;
    std::string communication_error;
};

struct MultiAgentResult {
    AgentWorkerResult first;
    AgentWorkerResult second;

    bool success() const noexcept;
};

struct MultiAgentConfig {
    std::string first_agent_id{"agent_1"};
    std::string second_agent_id{"agent_2"};
};

class MultiAgentCoordinator {
  public:
    MultiAgentCoordinator(AgentLoop &first_agent,
                          AgentLoop &second_agent,
                          MultiAgentConfig config = {});

    MultiAgentResult run(const ParallelAgentTask &task);

  private:
    AgentLoop &first_agent_;
    AgentLoop &second_agent_;
    MultiAgentConfig config_;
};

} // namespace oop_agent::agent
