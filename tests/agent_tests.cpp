#include "agent/agent_loop.h"
#include "skills/skill_loader.h"
#include "tools/calculator_tool.h"
#include "tools/tool_registry.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using oop_agent::agent::ActionType;
using oop_agent::agent::AgentLoop;
using oop_agent::agent::AgentLoopConfig;
using oop_agent::client::ChatRequest;
using oop_agent::client::ChatResponse;
using oop_agent::client::LLMClient;
using oop_agent::skills::SkillLoader;
using oop_agent::tools::CalculatorTool;
using oop_agent::tools::ToolRegistry;

void expect(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class ScriptedClient final : public LLMClient {
  public:
    explicit ScriptedClient(std::vector<ChatResponse> responses)
        : responses_(std::move(responses)) {}

    ChatResponse chat(const ChatRequest &request) override {
        requests.push_back(request);
        if (next_response_ >= responses_.size()) {
            return {false, {}, {}, "no scripted response remains"};
        }
        return responses_[next_response_++];
    }

    std::vector<ChatRequest> requests;

  private:
    std::vector<ChatResponse> responses_;
    std::size_t next_response_{0};
};

ChatResponse successfulResponse(std::string content,
                                long latency = 5,
                                std::int64_t prompt_tokens = 10,
                                std::int64_t completion_tokens = 4) {
    ChatResponse response;
    response.success = true;
    response.content = std::move(content);
    response.latency_ms = latency;
    response.prompt_tokens = prompt_tokens;
    response.completion_tokens = completion_tokens;
    return response;
}

std::filesystem::path createSkillDirectory(const std::string &name) {
    const auto directory = std::filesystem::current_path() / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    std::ofstream planner(directory / "task_planner.md");
    planner << "# Planner\n\n"
               "Keywords: plan, task, buoc\n\n"
               "- Break work into verifiable steps.\n";

    std::ofstream tools(directory / "tool_usage.md");
    tools << "# Tool Usage\n\n"
             "Keywords: calculator, calculate, tinh\n\n"
             "- Use calculator for arithmetic.\n";

    return directory;
}

void testSkillLoadingAndVietnameseKeywordSelection() {
    const auto directory = createSkillDirectory("skill-loader-test-workspace");
    SkillLoader loader(directory);

    expect(loader.loadSkills() == 2, "SkillLoader should load every markdown file");
    const auto selected = loader.selectSkills(u8"T\u00EDnh 15 * 17", 1);
    expect(selected.size() == 1 && selected.front().name == "tool_usage",
           "Vietnamese keyword matching should ignore accents and case");
    expect(selected.front().instructions.find("Keywords:") == std::string::npos,
           "keyword metadata should not be injected as instructions");

    std::filesystem::remove_all(directory);
}

void testReActToolCallThenFinalAnswer() {
    const auto directory = createSkillDirectory("agent-loop-test-workspace");
    SkillLoader loader(directory);

    ToolRegistry registry;
    expect(registry.registerFactory(
               "calculator", [] { return std::make_unique<CalculatorTool>(); }),
           "calculator registration should succeed");

    ScriptedClient client({
        successfulResponse(
            R"({"thought":"Calculate first","action":{"type":"tool_call","tool":"calculator","args":{"expression":"15*17"}}})"),
        successfulResponse(
            R"({"thought":"The observation contains the result","action":{"type":"final","answer":"255"}})"),
    });

    AgentLoopConfig config;
    config.max_steps = 4;
    AgentLoop agent(client, registry, loader, config);

    std::vector<oop_agent::agent::AgentStep> steps;
    agent.setStepHook([&steps](const auto &step) { steps.push_back(step); });

    const auto result = agent.run("Calculate 15 * 17 with the calculator tool");
    expect(result.success && result.final_answer == "255",
           "ReAct loop should return the scripted final answer");
    expect(result.steps_taken == 2 && steps.size() == 2,
           "hook should receive both the tool and final steps");
    expect(steps.front().action.type == ActionType::ToolCall &&
               steps.front().result.success && steps.front().result.output == "255",
           "Act should execute calculator through ToolRegistry");
    expect(client.requests.size() == 2 &&
               client.requests[1].messages.back().content.find("\"output\":\"255\"") !=
                   std::string::npos,
           "Observe should append the tool result to conversation history");
    expect(client.requests.front().messages.front().role == "system" &&
               client.requests.front().messages.front().content.find(
                   "[tool_usage]") != std::string::npos,
           "selected skill should be injected into the system prompt");
    expect(result.total_tokens == 28 && result.total_latency_ms == 10,
           "run should accumulate token and latency metrics");

    std::filesystem::remove_all(directory);
}

void testMalformedActionCanSelfCorrect() {
    const auto directory = createSkillDirectory("agent-correction-test-workspace");
    SkillLoader loader(directory);
    ToolRegistry registry;
    ScriptedClient client({
        successfulResponse("not-json"),
        successfulResponse(
            R"({"thought":"Corrected format","action":{"type":"final","answer":"ok"}})"),
    });

    AgentLoop agent(client, registry, loader);
    const auto result = agent.run("Complete this task");

    expect(result.success && result.steps_taken == 2,
           "malformed model output should receive one correction opportunity");
    expect(client.requests[1].messages.back().content.find("invalid_action") !=
               std::string::npos,
           "parse failure should become an observation in the next request");

    std::filesystem::remove_all(directory);
}

void testMaximumStepLimitIsGraceful() {
    const auto directory = createSkillDirectory("agent-limit-test-workspace");
    SkillLoader loader(directory);
    ToolRegistry registry;
    ScriptedClient client({
        successfulResponse("invalid"),
        successfulResponse("still invalid"),
    });

    AgentLoopConfig config;
    config.max_steps = 2;
    AgentLoop agent(client, registry, loader, config);
    const auto result = agent.run("Complete this task");

    expect(!result.success && result.steps_taken == 2 &&
               result.error_message.find("maximum step limit reached") !=
                   std::string::npos,
           "AgentLoop should stop gracefully at max_steps");

    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        testSkillLoadingAndVietnameseKeywordSelection();
        testReActToolCallThenFinalAnswer();
        testMalformedActionCanSelfCorrect();
        testMaximumStepLimitIsGraceful();
        std::cout << "All agent core tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
