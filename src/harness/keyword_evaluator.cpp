#include "keyword_evaluator.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace oop_agent::harness {

std::string_view KeywordEvaluator::name() const noexcept {
    return "keyword";
}

std::string KeywordEvaluator::toLower(
    const std::string &text
) {
    std::string result = text;

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return result;
}

std::vector<std::string>
KeywordEvaluator::parseKeywords(
    const EvaluationInput &input
) {
    const auto iterator =
        input.metadata.find("expected_keywords");

    if (iterator == input.metadata.end()) {
        return {};
    }

    const std::string &keywordData =
        iterator->second;

    // Ưu tiên định dạng JSON:
    // ["src", "skills", "benchmark"]
    try {
        const nlohmann::json jsonKeywords =
            nlohmann::json::parse(keywordData);

        if (jsonKeywords.is_array()) {
            return jsonKeywords.get<
                std::vector<std::string>
            >();
        }
    }
    catch (const nlohmann::json::exception &) {
        // Nếu không phải JSON thì đọc dạng phân cách bằng dấu phẩy.
    }

    std::vector<std::string> keywords;
    std::stringstream stream(keywordData);
    std::string keyword;

    while (std::getline(stream, keyword, ',')) {
        const auto first =
            keyword.find_first_not_of(" \t");

        const auto last =
            keyword.find_last_not_of(" \t");

        if (first != std::string::npos) {
            keywords.push_back(
                keyword.substr(first, last - first + 1)
            );
        }
    }

    return keywords;
}

EvaluationResult KeywordEvaluator::evaluate(
    const EvaluationInput &input
) const {
    const std::vector<std::string> keywords =
        parseKeywords(input);

    if (keywords.empty()) {
        return {
            false,
            0.0,
            "No expected_keywords were provided."
        };
    }

    const std::string normalizedOutput =
        toLower(input.actual_output);

    std::size_t matchedKeywords = 0;
    std::vector<std::string> missingKeywords;

    for (const std::string &keyword : keywords) {
        const std::string normalizedKeyword =
            toLower(keyword);

        if (
            normalizedOutput.find(normalizedKeyword)
            != std::string::npos
        ) {
            ++matchedKeywords;
        }
        else {
            missingKeywords.push_back(keyword);
        }
    }

    const double score =
        static_cast<double>(matchedKeywords)
        / static_cast<double>(keywords.size());

    const bool passed =
        matchedKeywords == keywords.size();

    std::ostringstream feedback;

    feedback
        << "Matched "
        << matchedKeywords
        << "/"
        << keywords.size()
        << " expected keywords.";

    if (!missingKeywords.empty()) {
        feedback << " Missing: ";

        for (std::size_t index = 0;
             index < missingKeywords.size();
             ++index) {
            feedback << missingKeywords[index];

            if (index + 1 < missingKeywords.size()) {
                feedback << ", ";
            }
        }
    }

    return {
        passed,
        score,
        feedback.str()
    };
}

} // namespace oop_agent::harness
