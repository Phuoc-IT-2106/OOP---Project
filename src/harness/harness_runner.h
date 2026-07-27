// Placeholder: HarnessRunner declaration for setup, run, evaluate, and record.
#ifndef HARNESS_RUNNER_H
#define HARNESS_RUNNER_H

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../agent/agent_loop.h"
#include "../environment/environment.h"
#include "benchmark_task.h"
#include "evaluator.h"
#include "trajectory_recorder.h"

class TaskResult {
private:
    std::string taskId;
    EvaluationResult evaluation;
    long long totalTimeMs;

public:
    TaskResult(
        const std::string& taskId,
        const EvaluationResult& evaluation,
        long long totalTimeMs
    )
        : taskId(taskId),
          evaluation(evaluation),
          totalTimeMs(totalTimeMs) {
    }

    const std::string& getTaskId() const {
        return taskId;
    }

    const EvaluationResult& getEvaluation() const {
        return evaluation;
    }

    nlohmann::json toJson() const {
        return {
            {"task_id", taskId},
            {"success", evaluation.isSuccess()},
            {"score", evaluation.getScore()},
            {"message", evaluation.getMessage()},
            {"total_time_ms", totalTimeMs}
        };
    }
};

class BatchResult {
private:
    int totalTasks;
    int passedTasks;
    int failedTasks;
    std::vector<TaskResult> taskResults;

public:
    BatchResult()
        : totalTasks(0),
          passedTasks(0),
          failedTasks(0) {
    }

    void addResult(const TaskResult& result) {
        taskResults.push_back(result);
        totalTasks++;

        if (result.getEvaluation().isSuccess()) {
            passedTasks++;
        }
        else {
            failedTasks++;
        }
    }

    double getSuccessRate() const {
        if (totalTasks == 0) {
            return 0.0;
        }

        return static_cast<double>(passedTasks)
            / static_cast<double>(totalTasks)
            * 100.0;
    }

    nlohmann::json toJson() const {
        nlohmann::json resultArray =
            nlohmann::json::array();

        for (const TaskResult& result : taskResults) {
            resultArray.push_back(result.toJson());
        }

        return {
            {"total_tasks", totalTasks},
            {"passed_tasks", passedTasks},
            {"failed_tasks", failedTasks},
            {"success_rate", getSuccessRate()},
            {"results", resultArray}
        };
    }
};

class HarnessRunner {
private:
    AgentLoop& agentLoop;
    Environment& environment;
    TrajectoryRecorder trajectoryRecorder;

    std::map<
        std::string,
        std::unique_ptr<Evaluator>
    > evaluators;

    std::filesystem::path resultDirectory;

    Evaluator& findEvaluator(
        const std::string& evaluationType
    );

    void saveBatchResult(
        const BatchResult& batchResult
    ) const;

public:
    HarnessRunner(
        AgentLoop& agentLoop,
        Environment& environment,
        const std::filesystem::path& resultDirectory
    );

    void registerEvaluator(
        const std::string& evaluationType,
        std::unique_ptr<Evaluator> evaluator
    );

    TaskResult runTask(
        const BenchmarkTask& task
    );

    BatchResult runBatch(
        const std::vector<BenchmarkTask>& tasks
    );
};

#endif
