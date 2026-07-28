#pragma once

#include "agent/agent_loop.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace oop_agent::harness {

struct TrajectoryStep {
    std::size_t step_id{0};
    std::string thought;
    std::string action_type;
    std::string tool_name;
    nlohmann::json arguments;
    std::string final_answer;
    bool tool_success{false};
    std::string tool_output;
    std::string tool_error;
    std::optional<int> tool_exit_code;
    std::string raw_response;
    std::int64_t tokens_used{0};
    long latency_ms{0};

    nlohmann::json toJson() const;
};

class Trajectory {
  public:
    Trajectory(std::string task_id, std::string model);

    void addStep(const agent::AgentStep &agent_step);
    void setResult(const agent::AgentRunResult &result);
    void setEvaluation(bool passed, double score, std::string feedback);
    void setWallTimeMs(std::int64_t total_time_ms);

    const std::string &taskId() const noexcept;
    nlohmann::json toJson() const;

  private:
    std::string task_id_;
    std::string model_;
    bool agent_success_{false};
    bool evaluation_passed_{false};
    double evaluation_score_{0.0};
    std::string evaluation_feedback_;
    std::string final_answer_;
    std::string error_message_;
    std::size_t steps_taken_{0};
    std::int64_t total_tokens_{0};
    std::int64_t total_time_ms_{0};
    std::vector<TrajectoryStep> steps_;
};

} // namespace oop_agent::harness
