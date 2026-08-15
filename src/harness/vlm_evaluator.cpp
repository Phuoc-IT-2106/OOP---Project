#include "vlm_evaluator.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace oop_agent::harness {

std::string_view VLMEvaluator::name() const noexcept {
    return "vlm";
}

std::string VLMEvaluator::toLower(const std::string &text) {
    std::string result = text;
    std::transform(result.begin(),
                   result.end(),
                   result.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return result;
}

std::vector<std::string> VLMEvaluator::parseList(const std::string &value) {
    std::vector<std::string> values;
    if (!value.empty() && value.front() == '[') {
        bool in_string = false;
        bool escaped = false;
        std::string current;
        for (const char character : value) {
            if (!in_string) {
                if (character == '"') {
                    in_string = true;
                    current.clear();
                }
                continue;
            }

            if (escaped) {
                current += character;
                escaped = false;
                continue;
            }
            if (character == '\\') {
                escaped = true;
                continue;
            }
            if (character == '"') {
                in_string = false;
                values.push_back(current);
                continue;
            }
            current += character;
        }
        if (!values.empty()) {
            return values;
        }
    }

    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const auto first = item.find_first_not_of(" \t");
        const auto last = item.find_last_not_of(" \t");
        if (first != std::string::npos) {
            values.push_back(item.substr(first, last - first + 1));
        }
    }
    return values;
}

EvaluationResult VLMEvaluator::evaluate(const EvaluationInput &input) const {
    const std::string normalized_output = toLower(input.actual_output);
    std::size_t checks = 0;
    std::size_t passed_checks = 0;
    std::vector<std::string> missing;

    const auto action = input.metadata.find("expected_action");
    if (action != input.metadata.end() && !action->second.empty()) {
        ++checks;
        const std::string expected_action = toLower(action->second);
        if (normalized_output.find(expected_action) != std::string::npos) {
            ++passed_checks;
        } else {
            missing.push_back("action=" + action->second);
        }
    }

    const auto keywords = input.metadata.find("expected_keywords");
    if (keywords != input.metadata.end() && !keywords->second.empty()) {
        for (const auto &keyword : parseList(keywords->second)) {
            ++checks;
            const std::string expected_keyword = toLower(keyword);
            if (normalized_output.find(expected_keyword) != std::string::npos) {
                ++passed_checks;
            } else {
                missing.push_back(keyword);
            }
        }
    }

    if (checks == 0) {
        return {false,
                0.0,
                "No expected_action or expected_keywords were provided."};
    }

    const double score =
        static_cast<double>(passed_checks) / static_cast<double>(checks);
    const bool passed = passed_checks == checks;

    std::ostringstream feedback;
    feedback << "Matched " << passed_checks << "/" << checks
             << " VLM expectations.";
    if (!missing.empty()) {
        feedback << " Missing: ";
        for (std::size_t index = 0; index < missing.size(); ++index) {
            if (index != 0) {
                feedback << ", ";
            }
            feedback << missing[index];
        }
    }
    return {passed, score, feedback.str()};
}

} // namespace oop_agent::harness
