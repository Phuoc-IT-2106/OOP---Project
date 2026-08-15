#pragma once

#include "evaluator.h"

#include <string>
#include <string_view>
#include <vector>

namespace oop_agent::harness {

class VLMEvaluator final : public Evaluator {
  public:
    std::string_view name() const noexcept override;
    EvaluationResult evaluate(const EvaluationInput &input) const override;

  private:
    static std::string toLower(const std::string &text);
    static std::vector<std::string> parseList(const std::string &value);
};

} // namespace oop_agent::harness
