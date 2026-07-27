// Placeholder: FunctionalEvaluator implementation.
#include "functional_evaluator.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace oop_agent::harness {

std::string_view
FunctionalEvaluator::name() const noexcept {
    return "functional";
}

std::string FunctionalEvaluator::quoteShellArgument(
    const std::string &value
) {
    std::string result = "'";

    for (const char character : value) {
        if (character == '\'') {
            result += "'\\''";
        }
        else {
            result += character;
        }
    }

    result += "'";
    return result;
}

EvaluationResult FunctionalEvaluator::evaluate(
    const EvaluationInput &input
) const {
    const auto iterator =
        input.metadata.find("eval_script");

    if (iterator == input.metadata.end() ||
        iterator->second.empty()) {
        return {
            false,
            0.0,
            "No eval_script was provided."
        };
    }

    const std::string &evalScript =
        iterator->second;

    if (evalScript == "placeholder") {
        return {
            false,
            0.0,
            "eval_script is still a placeholder."
        };
    }

    if (input.working_directory.empty()) {
        return {
            false,
            0.0,
            "Working directory is empty."
        };
    }

    /*
     * Chạy:
     *
     * cd 'working_directory' && eval_script
     *
     * FunctionalEvaluator chỉ đánh giá kết quả.
     * Nó không biết AgentLoop đã chạy như thế nào.
     */
    const std::string command =
        "cd "
        + quoteShellArgument(
            input.working_directory.string()
        )
        + " && "
        + evalScript;

    const int exitCode =
        std::system(command.c_str());

    if (exitCode == 0) {
        return {
            true,
            1.0,
            "Functional evaluation passed."
        };
    }

    std::ostringstream feedback;

    feedback
        << "Functional evaluation failed. "
        << "Command returned exit code "
        << exitCode
        << ".";

    return {
        false,
        0.0,
        feedback.str()
    };
}

} // namespace oop_agent::harness
