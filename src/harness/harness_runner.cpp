#include "harness_runner.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace oop_agent::harness {

nlohmann::json TaskResult::toJson() const {
    return {{"task_id", task_id},
            {"agent_success", agent_success},
            {"success", evaluation.passed},
            {"score", evaluation.score},
            {"feedback", evaluation.feedback},
            {"agent_error", agent_error},
            {"total_time_ms", total_time_ms}};
}

void BatchResult::add(TaskResult result) {
    if (result.evaluation.passed) {
        ++passed_tasks_;
    }
    results_.push_back(std::move(result));
}

std::size_t BatchResult::totalTasks() const noexcept {
    return results_.size();
}

std::size_t BatchResult::passedTasks() const noexcept {
    return passed_tasks_;
}

std::size_t BatchResult::failedTasks() const noexcept {
    return results_.size() - passed_tasks_;
}

double BatchResult::successRate() const noexcept {
    if (results_.empty()) {
        return 0.0;
    }
    return 100.0 * static_cast<double>(passed_tasks_) /
           static_cast<double>(results_.size());
}

const std::vector<TaskResult> &BatchResult::results() const noexcept {
    return results_;
}

nlohmann::json BatchResult::toJson() const {
    nlohmann::json serialized_results = nlohmann::json::array();
    for (const auto &result : results_) {
        serialized_results.push_back(result.toJson());
    }
    return {{"total_tasks", totalTasks()},
            {"passed_tasks", passedTasks()},
            {"failed_tasks", failedTasks()},
            {"success_rate", successRate()},
            {"results", std::move(serialized_results)}};
}

HarnessRunner::HarnessRunner(agent::AgentLoop &agent_loop,
                             Environment &environment,
                             std::filesystem::path result_directory,
                             std::string model_name)
    : agent_loop_(agent_loop),
      environment_(environment),
      trajectory_recorder_(result_directory),
      result_directory_(std::move(result_directory)),
      model_name_(std::move(model_name)) {
    if (model_name_.empty()) {
        throw std::invalid_argument("benchmark model name must not be empty");
    }
    std::filesystem::create_directories(result_directory_);
}

void HarnessRunner::registerEvaluator(std::unique_ptr<Evaluator> evaluator) {
    if (!evaluator) {
        throw std::invalid_argument("evaluator must not be null");
    }
    const std::string evaluator_name(evaluator->name());
    if (evaluator_name.empty()) {
        throw std::invalid_argument("evaluator name must not be empty");
    }
    evaluators_[evaluator_name] = std::move(evaluator);
}

Evaluator &HarnessRunner::findEvaluator(const std::string &evaluation_type) {
    const auto found = evaluators_.find(evaluation_type);
    if (found == evaluators_.end()) {
        throw std::runtime_error("no evaluator registered for type: " +
                                 evaluation_type);
    }
    return *found->second;
}

EvaluationInput HarnessRunner::makeEvaluationInput(
    const BenchmarkTask &task,
    const agent::AgentRunResult &agent_result) const {
    EvaluationInput input;
    input.task_id = task.id;
    input.instruction = task.instruction;
    input.actual_output = agent_result.final_answer;
    input.working_directory = environment_.workingDirectory();
    if (!task.eval_script.empty()) {
        input.metadata["eval_script"] = task.eval_script;
    }
    if (!task.expected_action.empty()) {
        input.metadata["expected_action"] = task.expected_action;
    }
    if (!task.expected_keywords.empty()) {
        input.metadata["expected_keywords"] =
            nlohmann::json(task.expected_keywords).dump();
    }
    return input;
}

TaskResult HarnessRunner::runTask(const BenchmarkTask &task) {
    if (task.id.empty() || task.instruction.empty() || task.eval_type.empty()) {
        throw std::invalid_argument("benchmark task is missing required fields");
    }
    if (task.max_steps == 0) {
        throw std::invalid_argument("benchmark task max_steps must be positive");
    }

    std::cout << "[" << task.id << "] " << task.description << '\n';
    Trajectory trajectory(task.id, model_name_);
    const auto started_at = std::chrono::steady_clock::now();
    bool environment_ready = false;

    try {
        environment_.setup();
        environment_ready = true;
        agent_loop_.setMaxSteps(task.max_steps);
        agent_loop_.setStepHook(
            [&trajectory](const agent::AgentStep &step) {
                trajectory.addStep(step);
            });

        const auto agent_result = agent_loop_.run(task.instruction);
        agent_loop_.setStepHook({});
        trajectory.setResult(agent_result);

        EvaluationResult evaluation;
        if (!agent_result.success) {
            evaluation = {false,
                          0.0,
                          "Agent failed before evaluation: " +
                              agent_result.error_message};
        } else {
            evaluation =
                findEvaluator(task.eval_type)
                    .evaluate(makeEvaluationInput(task, agent_result));
        }
        trajectory.setEvaluation(
            evaluation.passed, evaluation.score, evaluation.feedback);

        const auto finished_at = std::chrono::steady_clock::now();
        const auto total_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                finished_at - started_at)
                .count();
        trajectory.setWallTimeMs(total_time_ms);
        trajectory_recorder_.save(trajectory);

        environment_.cleanup();
        environment_ready = false;
        return {task.id,
                agent_result.success,
                std::move(evaluation),
                total_time_ms,
                agent_result.error_message};
    } catch (...) {
        agent_loop_.setStepHook({});
        if (environment_ready) {
            try {
                environment_.cleanup();
            } catch (...) {
                // Preserve the original exception.
            }
        }
        throw;
    }
}

BatchResult HarnessRunner::runBatch(
    const std::vector<BenchmarkTask> &tasks) {
    BatchResult batch_result;
    for (const auto &task : tasks) {
        try {
            auto result = runTask(task);
            std::cout << "  -> "
                      << (result.evaluation.passed ? "PASS" : "FAIL")
                      << " (" << result.evaluation.feedback << ")\n";
            batch_result.add(std::move(result));
        } catch (const std::exception &error) {
            std::cerr << "  -> ERROR: " << error.what() << '\n';
            batch_result.add(
                {task.id, false, {false, 0.0, error.what()}, 0, error.what()});
        }
    }
    saveBatchResult(batch_result);
    return batch_result;
}

void HarnessRunner::saveBatchResult(
    const BatchResult &batch_result) const {
    std::filesystem::create_directories(result_directory_);
    const auto output_path = result_directory_ / "benchmark_summary.json";
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("cannot create benchmark summary: " +
                                 output_path.string());
    }
    output << batch_result.toJson().dump(4) << '\n';
    if (!output.good()) {
        throw std::runtime_error("failed to write benchmark summary: " +
                                 output_path.string());
    }
}

} // namespace oop_agent::harness
