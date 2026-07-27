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

// Chuyển một object JSON thành BenchmarkTask
inline void from_json(
    const nlohmann::json &j,
    BenchmarkTask &task
) {
    j.at("id").get_to(task.id);
    j.at("description").get_to(task.description);
    j.at("instruction").get_to(task.instruction);
    j.at("eval_type").get_to(task.eval_type);

    task.eval_script =
        j.value("eval_script", "");

    task.expected_keywords =
        j.value(
            "expected_keywords",
            std::vector<std::string>{}
        );

    task.max_steps =
        j.value("max_steps", std::size_t{10});
}

// Đọc benchmark/tasks.json
inline std::vector<BenchmarkTask> loadBenchmarkTasks(
    const std::filesystem::path &path
) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(
            "Cannot find benchmark file: "
            + path.string()
        );
    }

    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open benchmark file."
        );
    }

    nlohmann::json jsonData;
    file >> jsonData;

    return jsonData.get<std::vector<BenchmarkTask>>();
}

} // namespace oop_agent::harness
