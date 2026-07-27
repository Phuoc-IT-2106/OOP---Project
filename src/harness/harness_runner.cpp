// Placeholder: HarnessRunner implementation.
#include "harness_runner.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

HarnessRunner::HarnessRunner(
    AgentLoop& agentLoop,
    Environment& environment,
    const std::filesystem::path& resultDirectory
)
    : agentLoop(agentLoop),
      environment(environment),
      trajectoryRecorder(resultDirectory),
      resultDirectory(resultDirectory) {
    std::filesystem::create_directories(
        resultDirectory
    );
}

void HarnessRunner::registerEvaluator(
    const std::string& evaluationType,
    std::unique_ptr<Evaluator> evaluator
) {
    if (evaluator == nullptr) {
        throw std::invalid_argument(
            "Evaluator khong duoc la nullptr."
        );
    }

    evaluators[evaluationType] =
        std::move(evaluator);
}

Evaluator& HarnessRunner::findEvaluator(
    const std::string& evaluationType
) {
    auto iterator =
        evaluators.find(evaluationType);

    if (iterator == evaluators.end()) {
        throw std::runtime_error(
            "Khong tim thay evaluator cho loai: "
            + evaluationType
        );
    }

    return *(iterator->second);
}

TaskResult HarnessRunner::runTask(
    const BenchmarkTask& task
) {
    std::cout
        << "Dang chay task: "
        << task.getId()
        << '\n';

    Trajectory trajectory(
        task.getId(),
        agentLoop.getModelName()
    );

    const auto startTime =
        std::chrono::steady_clock::now();

    bool environmentReady = false;

    try {

        environment.setup();
        environmentReady = true;

        agentLoop.setMaxSteps(
            task.getMaxSteps()
        );

        // Harness ghi nhận Step qua hook.
        agentLoop.setStepHook(
            [&trajectory](const Step& step) {
                trajectory.addStep(step);
            }
        );

        AgentResult agentResult =
            agentLoop.run(
                task.getInstruction()
            );

        Evaluator& evaluator =
            findEvaluator(
                task.getEvaluationType()
            );

        EvaluationResult evaluation =
            evaluator.evaluate(
                task,
                agentResult
            );

        const auto endTime =
            std::chrono::steady_clock::now();

        const long long totalTimeMs =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(endTime - startTime).count();

        trajectory.setSuccess(
            evaluation.isSuccess()
        );

        trajectory.setTotalTimeMs(
            totalTimeMs
        );
        trajectoryRecorder.save(
            trajectory
        );

        // 7. Cleanup environment
        environment.cleanup();
        environmentReady = false;

        return TaskResult(
            task.getId(),
            evaluation,
            totalTimeMs
        );
    }
    catch (...) {
        const auto endTime =
            std::chrono::steady_clock::now();

        const long long totalTimeMs =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(endTime - startTime).count();

        trajectory.setSuccess(false);
        trajectory.setTotalTimeMs(
            totalTimeMs
        );

        try {
            trajectoryRecorder.save(
                trajectory
            );
        }
        catch (...) {
            // Giữ lại exception ban đầu.
        }

        if (environmentReady) {
            try {
                environment.cleanup();
            }
            catch (...) {
                // Giữ lại exception ban đầu.
            }
        }

        throw;
    }
}

BatchResult HarnessRunner::runBatch(
    const std::vector<BenchmarkTask>& tasks
) {
    BatchResult batchResult;

    for (const BenchmarkTask& task : tasks) {
        try {
            TaskResult taskResult =
                runTask(task);

            batchResult.addResult(
                taskResult
            );
        }
        catch (const std::exception& exception) {
            std::cerr
                << "Task "
                << task.getId()
                << " that bai: "
                << exception.what()
                << '\n';

            EvaluationResult failedEvaluation(
                false,
                0.0,
                exception.what()
            );

            TaskResult failedTask(
                task.getId(),
                failedEvaluation,
                0
            );

            batchResult.addResult(
                failedTask
            );
        }
    }

    saveBatchResult(
        batchResult
    );

    return batchResult;
}

void HarnessRunner::saveBatchResult(
    const BatchResult& batchResult
) const {
    const std::filesystem::path outputPath =
        resultDirectory
        / "benchmark_summary.json";

    std::ofstream outputFile(outputPath);

    if (!outputFile.is_open()) {
        throw std::runtime_error(
            "Khong the tao benchmark_summary.json"
        );
    }

    outputFile
        << batchResult.toJson().dump(4);
}
