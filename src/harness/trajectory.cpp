#include "trajectory.h"

#include <utility>

namespace oop_agent::harness {
namespace {

std::string actionTypeToString(agent::ActionType type) {
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
    nlohmann::json action = {{"type", action_type}};
    if (!tool_name.empty()) {
        action["tool"] = tool_name;
    }
    if (!arguments.empty()) {
        action["args"] = arguments;
    }
    if (!final_answer.empty()) {
        action["final_answer"] = final_answer;
    }

    nlohmann::json tool_result = {{"success", tool_success},
                                  {"output", tool_output},
                                  {"error", tool_error}};
    if (tool_exit_code.has_value()) {
        tool_result["exit_code"] = *tool_exit_code;
    }

    return {{"step_id", step_id},
            {"thought", thought},
            {"action", std::move(action)},
            {"tool_result", std::move(tool_result)},
            {"raw_response", raw_response},
            {"tokens_used", tokens_used},
            {"latency_ms", latency_ms}};
}

Trajectory::Trajectory(std::string task_id, std::string model)
    : task_id_(std::move(task_id)), model_(std::move(model)) {}

void Trajectory::addStep(const agent::AgentStep &agent_step) {
    TrajectoryStep step;
    step.step_id = agent_step.step_id;
    step.thought = agent_step.thought;
    step.action_type = actionTypeToString(agent_step.action.type);
    step.tool_name = agent_step.action.tool_name;
    step.arguments = nlohmann::json::object();
    for (const auto &[name, value] : agent_step.action.arguments) {
        step.arguments[name] = value;
    }
    step.final_answer = agent_step.action.final_answer;
    step.tool_success = agent_step.result.success;
    step.tool_output = agent_step.result.output;
    step.tool_error = agent_step.result.error_message;
    step.tool_exit_code = agent_step.result.exit_code;
    step.raw_response = agent_step.raw_response;
    step.tokens_used = agent_step.tokens_used;
    step.latency_ms = agent_step.latency_ms;
    steps_.push_back(std::move(step));
}

void Trajectory::setResult(const agent::AgentRunResult &result) {
    agent_success_ = result.success;
    final_answer_ = result.final_answer;
    error_message_ = result.error_message;
    steps_taken_ = result.steps_taken;
    total_tokens_ = result.total_tokens;
}

void Trajectory::setEvaluation(bool passed,
                               double score,
                               std::string feedback) {
    evaluation_passed_ = passed;
    evaluation_score_ = score;
    evaluation_feedback_ = std::move(feedback);
}

void Trajectory::setWallTimeMs(std::int64_t total_time_ms) {
    total_time_ms_ = total_time_ms;
}

const std::string &Trajectory::taskId() const noexcept {
    return task_id_;
}

nlohmann::json Trajectory::toJson() const {
    nlohmann::json serialized_steps = nlohmann::json::array();
    for (const auto &step : steps_) {
        serialized_steps.push_back(step.toJson());
    }
    return {{"task_id", task_id_},
            {"model", model_},
            {"success", evaluation_passed_},
            {"agent_success", agent_success_},
            {"evaluation",
             {{"passed", evaluation_passed_},
              {"score", evaluation_score_},
              {"feedback", evaluation_feedback_}}},
            {"final_answer", final_answer_},
            {"error_message", error_message_},
            {"steps_taken", steps_taken_},
            {"total_tokens", total_tokens_},
            {"total_time_ms", total_time_ms_},
            {"steps", std::move(serialized_steps)}};
}

} // namespace oop_agent::harness
