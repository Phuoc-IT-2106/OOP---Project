#pragma once

#include "evaluator.h"

#include <string_view>
#include <vector>

namespace oop_agent::harness {

class KeywordEvaluator final : public Evaluator {
  public:
    std::string_view name() const noexcept override;

    EvaluationResult evaluate(
        const EvaluationInput &input
    ) const override;

  private:
    static std::vector<std::string> parseKeywords(
        const EvaluationInput &input
    );

    static std::string toLower(
        const std::string &text
    );
};

} // namespace oop_agent::harness
