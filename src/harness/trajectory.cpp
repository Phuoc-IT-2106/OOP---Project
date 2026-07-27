// Placeholder: Trajectory implementation.
#include "trajectory.h"

#include <utility>

namespace oop_agent::harness {

namespace {

std::string actionTypeToString(
    const agent::ActionType type
) {
    switch (type) {
        case agent::ActionType::ToolCall:
            return "tool_call";

        case agent::ActionType::FinalAnswer:
            return "final_answer";

        case agent::ActionType::Invalid:
        default:
            return "invalid";
    }
}

} // namespace

nlohmann::json TrajectoryStep::toJson() const {
    nlohmann::json actionJson = {
        {"type", action_type}
    };

    if (!tool_name.empty()) {
        actionJson["tool"] = tool_name;
    }

    if (!arguments.empty()) {
        actionJson["arguments"] = arguments;
    }

    if (!final_answer.empty()) {
        actionJson["final_answer"] =
            final_answer;
    }

    return {
        {"step_id", step_id},
        {"thought", thought},
        {"action", actionJson},
        {"raw_response", raw_response},
        {"tokens_used", tokens_used},
        {"latency_ms", latency_ms}
    };
}

Trajectory::Trajectory(
    std::string task_id,
    std::string model
)
    : task_id_(std::move(task_id)),
      model_(std::move(model)) {
}

void Trajectory::addStep(
    const agent::AgentStep &agent_step
) {
    TrajectoryStep step;

    step.step_id =
        agent_step.step_id;

    step.thought =
        agent_step.thought;

    step.action_type =
        actionTypeToString(
            agent_step.action.type
        );

    step.tool_name =
        agent_step.action.tool_name;

    /*
     * tools::ToolArguments chưa được cung cấp đầy đủ.
     * Nếu ToolArguments là nlohmann::json thì thay bằng:
     *
     * step.arguments = agent_step.action.arguments;
     *
     * Hiện tại để object rỗng nhằm giữ code trajectory
     * độc lập với cách ToolArguments được triển khai.
     */
    step.arguments =
        nlohmann::json::object();

    step.final_answer =
        agent_step.action.final_answer;

    step.raw_response =
        agent_step.raw_response;

    step.tokens_used =
        agent_step.tokens_used;

    step.latency_ms =
        agent_step.latency_ms;

    steps_.push_back(
        std::move(step)
    );
}

void Trajectory::setResult(
    const agent::AgentRunResult &result
) {
    agent_success_ =
        result.success;

    final_answer_ =
        result.final_answer;

    error_message_ =
        result.error_message;

    steps_taken_ =
        result.steps_taken;

    total_tokens_ =
        result.total_tokens;

    total_time_ms_ =
        result.total_latency_ms;
}

void Trajectory::setEvaluation(
    bool passed,
    double score,
    std::string feedback
) {
    evaluation_passed_ = passed;
    evaluation_score_ = score;
    evaluation_feedback_ =
        std::move(feedback);
}

const std::string &
Trajectory::taskId() const noexcept {
    return task_id_;
}

nlohmann::json Trajectory::toJson() const {
    nlohmann::json stepArray =
        nlohmann::json::array();

    for (const TrajectoryStep &step : steps_) {
        stepArray.push_back(
            step.toJson()
        );
    }

    return {
        {"task_id", task_id_},
        {"model", model_},
        {"agent_success", agent_success_},
        {"evaluation", {
            {"passed", evaluation_passed_},
            {"score", evaluation_score_},
            {"feedback", evaluation_feedback_}
        }},
        {"final_answer", final_answer_},
        {"error_message", error_message_},
        {"steps_taken", steps_taken_},
        {"total_tokens", total_tokens_},
        {"total_time_ms", total_time_ms_},
        {"steps", stepArray}
    };
}

} // namespace oop_agent::harness
