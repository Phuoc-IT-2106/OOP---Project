#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace oop_agent::harness {

struct EvaluationInput {
    std::string task_id;
    std::string instruction;
    std::string actual_output;
    std::string expected_output;
    std::filesystem::path working_directory;

    // Evaluator-specific data (keywords, eval_script, thresholds, ...) stays
    // outside HarnessRunner so new strategies do not change its interface.
    std::unordered_map<std::string, std::string> metadata;
};

struct EvaluationResult {
    bool passed{false};
    // Evaluators use a normalized [0.0, 1.0] score for batch aggregation.
    double score{0.0};
    std::string feedback;
};

// Strategy interface: HarnessRunner can select KeywordEvaluator or
// FunctionalEvaluator at runtime through the same evaluate() contract.
class Evaluator {
  public:
    virtual ~Evaluator() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual EvaluationResult evaluate(const EvaluationInput &input) const = 0;
};

} // namespace oop_agent::harness
