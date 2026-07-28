#pragma once

#include "agent/agent_loop.h"
#include "benchmark_task.h"
#include "environment.h"
#include "evaluator.h"
#include "trajectory_recorder.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace oop_agent::harness {

struct TaskResult {
    std::string task_id;
    bool agent_success{false};
    EvaluationResult evaluation;
    std::int64_t total_time_ms{0};
    std::string agent_error;

    nlohmann::json toJson() const;
};

class BatchResult {
  public:
    void add(TaskResult result);

    std::size_t totalTasks() const noexcept;
    std::size_t passedTasks() const noexcept;
    std::size_t failedTasks() const noexcept;
    double successRate() const noexcept;
    const std::vector<TaskResult> &results() const noexcept;
    nlohmann::json toJson() const;

  private:
    std::vector<TaskResult> results_;
    std::size_t passed_tasks_{0};
};

class HarnessRunner {
  public:
    HarnessRunner(agent::AgentLoop &agent_loop,
                  Environment &environment,
                  std::filesystem::path result_directory,
                  std::string model_name);

    void registerEvaluator(std::unique_ptr<Evaluator> evaluator);

    TaskResult runTask(const BenchmarkTask &task);
    BatchResult runBatch(const std::vector<BenchmarkTask> &tasks);

  private:
    Evaluator &findEvaluator(const std::string &evaluation_type);
    EvaluationInput makeEvaluationInput(
        const BenchmarkTask &task,
        const agent::AgentRunResult &agent_result) const;
    void saveBatchResult(const BatchResult &batch_result) const;

    agent::AgentLoop &agent_loop_;
    Environment &environment_;
    TrajectoryRecorder trajectory_recorder_;
    std::filesystem::path result_directory_;
    std::string model_name_;
    std::map<std::string, std::unique_ptr<Evaluator>> evaluators_;
};

} // namespace oop_agent::harness
