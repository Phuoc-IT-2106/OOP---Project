#include "agent/agent_loop.h"
#include "client/llm_client.h"
#include "harness/functional_evaluator.h"
#include "harness/harness_runner.h"
#include "harness/keyword_evaluator.h"
#include "skills/skill_loader.h"
#include "tools/file_tool.h"
#include "tools/tool_registry.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

using oop_agent::client::ChatRequest;
using oop_agent::client::ChatResponse;
using oop_agent::client::LLMClient;

void expect(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class ScriptedClient final : public LLMClient {
  public:
    explicit ScriptedClient(std::vector<ChatResponse> responses)
        : responses_(std::move(responses)) {}

    ChatResponse chat(const ChatRequest &) override {
        if (next_ >= responses_.size()) {
            return {false, {}, {}, "no scripted response remains"};
        }
        return responses_[next_++];
    }

  private:
    std::vector<ChatResponse> responses_;
    std::size_t next_{0};
};

ChatResponse response(std::string content) {
    ChatResponse result;
    result.success = true;
    result.content = std::move(content);
    result.prompt_tokens = 3;
    result.completion_tokens = 2;
    return result;
}

void testBatchEvaluationAndJsonOutput() {
    namespace fs = std::filesystem;
    const fs::path root = fs::current_path() / "harness-test-workspace";
    const fs::path workspace = root / "workspace";
    const fs::path results = root / "results";
    const fs::path skills = root / "skills";
    fs::remove_all(root);
    fs::create_directories(workspace);
    fs::create_directories(skills);
    std::ofstream(skills / "test.md")
        << "# Test\n\nKeywords: task, artifact\n\n- Follow the task.\n";

    ScriptedClient client({
        response(
            R"({"thought":"answer","action":{"type":"final","answer":"alpha result"}})"),
        response(
            R"({"thought":"write","action":{"type":"tool_call","tool":"write_file","args":{"path":"artifact.txt","content":"ok"}}})"),
        response(
            R"({"thought":"finish","action":{"type":"final","answer":"done"}})"),
    });

    oop_agent::tools::FileToolConfig files;
    files.root_directory = workspace;
    oop_agent::tools::ToolRegistry registry;
    registry.registerFactory("write_file", [files] {
        return std::make_unique<oop_agent::tools::WriteFileTool>(files);
    });

    oop_agent::skills::SkillLoader skill_loader(skills);
    oop_agent::agent::AgentLoop agent(client, registry, skill_loader);
    oop_agent::harness::NativeEnvironment environment(workspace);
    oop_agent::harness::HarnessRunner harness(
        agent, environment, results, "fake-model");
    harness.registerEvaluator(
        std::make_unique<oop_agent::harness::KeywordEvaluator>());
    harness.registerEvaluator(
        std::make_unique<oop_agent::harness::FunctionalEvaluator>());

    std::vector<oop_agent::harness::BenchmarkTask> tasks(2);
    tasks[0].id = "keyword_task";
    tasks[0].description = "keyword";
    tasks[0].instruction = "Return alpha";
    tasks[0].eval_type = "keyword";
    tasks[0].expected_keywords = {"alpha"};
    tasks[0].max_steps = 2;

    tasks[1].id = "functional_task";
    tasks[1].description = "write artifact";
    tasks[1].instruction = "Write artifact";
    tasks[1].eval_type = "functional";
    tasks[1].eval_script = "file_contains|artifact.txt|ok";
    tasks[1].max_steps = 3;

    const auto batch = harness.runBatch(tasks);
    expect(batch.totalTasks() == 2 && batch.passedTasks() == 2 &&
               batch.successRate() == 100.0,
           "HarnessRunner should aggregate evaluator success rate");
    expect(fs::exists(results / "benchmark_summary.json") &&
               fs::exists(results / "trajectory_keyword_task.json") &&
               fs::exists(results / "trajectory_functional_task.json"),
           "HarnessRunner should write summary and per-task trajectories");

    std::ifstream summary_file(results / "benchmark_summary.json");
    nlohmann::json summary;
    summary_file >> summary;
    expect(summary["total_tasks"] == 2 &&
               summary["success_rate"] == 100.0,
           "benchmark summary should contain aggregate metrics");

    std::ifstream trajectory_file(
        results / "trajectory_functional_task.json");
    nlohmann::json trajectory;
    trajectory_file >> trajectory;
    expect(trajectory["steps"][0]["action"]["args"]["path"] ==
               "artifact.txt" &&
               trajectory["steps"][0]["tool_result"]["success"] == true,
           "trajectory should record tool arguments and result");

    fs::remove_all(root);
}

} // namespace

int main() {
    try {
        testBatchEvaluationAndJsonOutput();
        std::cout << "All harness tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
