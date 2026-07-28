#pragma once

#include "evaluator.h"

#include <string>
#include <string_view>

namespace oop_agent::harness {

class FunctionalEvaluator final : public Evaluator {
  public:
    std::string_view name() const noexcept override;

    EvaluationResult evaluate(
        const EvaluationInput &input
    ) const override;

  private:
    static std::string quoteShellArgument(
        const std::string &value
    );
};

} // namespace oop_agent::harness
