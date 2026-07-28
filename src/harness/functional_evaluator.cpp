#include "functional_evaluator.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace oop_agent::harness {
namespace {

constexpr std::string_view file_contains_prefix{"file_contains|"};
constexpr std::string_view file_nonempty_prefix{"file_nonempty|"};

bool isWithin(const std::filesystem::path &root,
              const std::filesystem::path &candidate) {
    auto root_part = root.begin();
    auto candidate_part = candidate.begin();
    for (; root_part != root.end(); ++root_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *root_part != *candidate_part) {
            return false;
        }
    }
    return true;
}

std::filesystem::path resolveArtifact(
    const std::filesystem::path &working_directory,
    const std::string &relative_path) {
    const auto root = std::filesystem::weakly_canonical(
        std::filesystem::absolute(working_directory));
    const auto artifact =
        std::filesystem::weakly_canonical(root / relative_path);
    if (!isWithin(root, artifact)) {
        throw std::invalid_argument(
            "functional evaluator path escapes working directory");
    }
    return artifact;
}

EvaluationResult evaluatePortableFileCheck(
    const std::string &script,
    const std::filesystem::path &working_directory) {
    if (script.rfind(file_nonempty_prefix, 0) == 0) {
        const auto relative_path =
            script.substr(file_nonempty_prefix.size());
        const auto path = resolveArtifact(working_directory, relative_path);
        const bool passed =
            std::filesystem::is_regular_file(path) &&
            std::filesystem::file_size(path) > 0;
        return {passed,
                passed ? 1.0 : 0.0,
                passed ? "Expected non-empty file exists."
                       : "Expected file is missing or empty: " + relative_path};
    }

    const auto payload = script.substr(file_contains_prefix.size());
    const auto separator = payload.find('|');
    if (separator == std::string::npos) {
        return {false, 0.0, "Malformed file_contains evaluator script."};
    }
    const auto relative_path = payload.substr(0, separator);
    const auto expected_text = payload.substr(separator + 1);
    std::ifstream input(resolveArtifact(working_directory, relative_path));
    if (!input) {
        return {false, 0.0, "Expected file is missing: " + relative_path};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    const bool passed = contents.str().find(expected_text) != std::string::npos;
    return {passed,
            passed ? 1.0 : 0.0,
            passed ? "Expected file content found."
                   : "Expected text not found in " + relative_path};
}

} // namespace

std::string_view FunctionalEvaluator::name() const noexcept {
    return "functional";
}

std::string FunctionalEvaluator::quoteShellArgument(
    const std::string &value) {
    std::string result = "'";
    for (const char character : value) {
        if (character == '\'') {
            result += "'\\''";
        } else {
            result += character;
        }
    }
    result += "'";
    return result;
}

EvaluationResult FunctionalEvaluator::evaluate(
    const EvaluationInput &input) const {
    const auto script = input.metadata.find("eval_script");
    if (script == input.metadata.end() || script->second.empty()) {
        return {false, 0.0, "No eval_script was provided."};
    }
    if (input.working_directory.empty()) {
        return {false, 0.0, "Working directory is empty."};
    }

    // These two built-in checks make the bundled benchmark deterministic on
    // Windows and Linux. Other eval_script values remain valid shell scripts.
    if (script->second.rfind(file_contains_prefix, 0) == 0 ||
        script->second.rfind(file_nonempty_prefix, 0) == 0) {
        try {
            return evaluatePortableFileCheck(
                script->second, input.working_directory);
        } catch (const std::exception &error) {
            return {false, 0.0, error.what()};
        }
    }

#ifdef _WIN32
    const std::string command =
        "cd /D \"" + input.working_directory.string() +
        "\" && " + script->second;
#else
    const std::string command =
        "cd " + quoteShellArgument(input.working_directory.string()) +
        " && " + script->second;
#endif
    const int exit_code = std::system(command.c_str());
    if (exit_code == 0) {
        return {true, 1.0, "Functional evaluation passed."};
    }
    return {false,
            0.0,
            "Functional evaluation failed with exit code " +
                std::to_string(exit_code) + "."};
}

} // namespace oop_agent::harness
