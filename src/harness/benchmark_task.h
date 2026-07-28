#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace oop_agent::harness {

struct BenchmarkTask {
    std::string id;
    std::string description;
    std::string instruction;
    std::string eval_type;
    std::string eval_script;
    std::vector<std::string> expected_keywords;
    std::size_t max_steps{10};
};

inline void from_json(const nlohmann::json &json, BenchmarkTask &task) {
    json.at("id").get_to(task.id);
    json.at("description").get_to(task.description);
    json.at("instruction").get_to(task.instruction);
    json.at("eval_type").get_to(task.eval_type);
    task.eval_script = json.value("eval_script", "");
    task.expected_keywords =
        json.value("expected_keywords", std::vector<std::string>{});
    task.max_steps = json.value("max_steps", std::size_t{10});
}

inline std::vector<BenchmarkTask> loadBenchmarkTasks(
    const std::filesystem::path &path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open benchmark task file: " +
                                 path.string());
    }

    try {
        nlohmann::json document;
        input >> document;
        if (!document.is_array()) {
            throw std::runtime_error("benchmark task document must be an array");
        }
        return document.get<std::vector<BenchmarkTask>>();
    } catch (const nlohmann::json::exception &error) {
        throw std::runtime_error("invalid benchmark JSON in " + path.string() +
                                 ": " + error.what());
    }
}

} // namespace oop_agent::harness
