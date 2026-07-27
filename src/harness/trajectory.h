// Placeholder: Trajectory and Step data model declarations.
#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class Step {
private:
    int stepId;
    std::string thought;
    nlohmann::json action;
    std::string toolResult;
    int tokensUsed;
    long long latencyMs;

public:
    Step()
        : stepId(0),
          tokensUsed(0),
          latencyMs(0) {
    }

    Step(
        int stepId,
        const std::string& thought,
        const nlohmann::json& action,
        const std::string& toolResult,
        int tokensUsed,
        long long latencyMs
    )
        : stepId(stepId),
          thought(thought),
          action(action),
          toolResult(toolResult),
          tokensUsed(tokensUsed),
          latencyMs(latencyMs) {
    }

    int getTokensUsed() const {
        return tokensUsed;
    }

    nlohmann::json toJson() const {
        return {
            {"step_id", stepId},
            {"thought", thought},
            {"action", action},
            {"tool_result", toolResult},
            {"tokens_used", tokensUsed},
            {"latency_ms", latencyMs}
        };
    }
};

class Trajectory {
private:
    std::string taskId;
    std::string model;
    bool success;
    int totalTokens;
    long long totalTimeMs;
    std::vector<Step> steps;

public:
    Trajectory(
        const std::string& taskId,
        const std::string& model
    )
        : taskId(taskId),
          model(model),
          success(false),
          totalTokens(0),
          totalTimeMs(0) {
    }

    void addStep(const Step& step) {
        steps.push_back(step);
        totalTokens += step.getTokensUsed();
    }

    void setSuccess(bool success) {
        this->success = success;
    }

    void setTotalTimeMs(long long totalTimeMs) {
        this->totalTimeMs = totalTimeMs;
    }

    const std::string& getTaskId() const {
        return taskId;
    }

    nlohmann::json toJson() const {
        nlohmann::json stepArray =
            nlohmann::json::array();

        for (const Step& step : steps) {
            stepArray.push_back(step.toJson());
        }

        return {
            {"task_id", taskId},
            {"model", model},
            {"success", success},
            {"total_tokens", totalTokens},
            {"total_time_ms", totalTimeMs},
            {"steps", stepArray}
        };
    }
};

#endif
