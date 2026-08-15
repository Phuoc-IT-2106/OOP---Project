#include "agent/agent_loop.h"
#include "agent/loop_detector.h"
#include "agent/message_queue.h"
#include "agent/multi_agent_coordinator.h"
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
using oop_agent::agent::LoopDetector;
using oop_agent::agent::LoopDetectorConfig;
using oop_agent::agent::LoopKind;
using oop_agent::agent::LoopSeverity;
using oop_agent::agent::MessageQueue;
using oop_agent::agent::MultiAgentCoordinator;
using oop_agent::agent::ParallelAgentTask;
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
    const auto selected = loader.selectSkills("Tính 15 * 17", 1);
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

void testLoopDetectorRepeatAndPingPong() {
    LoopDetectorConfig config;
    config.repeat = {2, 3};
    config.ping_pong = {2, 3};
    config.max_history = 8;
    LoopDetector detector(config);

    expect(!detector.observe("A").detected(),
           "first action should not be a repeat loop");
    const auto repeat_warning = detector.observe("A");
    expect(repeat_warning.kind == LoopKind::Repeat &&
               repeat_warning.severity == LoopSeverity::Warning &&
               !repeat_warning.shouldStop(),
           "second identical action should reach repeat warning");
    const auto repeat_critical = detector.observe("A");
    expect(repeat_critical.kind == LoopKind::Repeat &&
               repeat_critical.severity == LoopSeverity::Critical &&
               repeat_critical.shouldStop(),
           "third identical action should reach repeat critical");

    detector.reset();
    expect(detector.historySize() == 0, "reset should clear loop history");
    detector.observe("A");
    detector.observe("B");
    detector.observe("A");
    const auto ping_pong_warning = detector.observe("B");
    expect(ping_pong_warning.kind == LoopKind::PingPong &&
               ping_pong_warning.severity == LoopSeverity::Warning,
           "A-B-A-B should reach ping-pong warning");
    detector.observe("A");
    const auto ping_pong_critical = detector.observe("B");
    expect(ping_pong_critical.kind == LoopKind::PingPong &&
               ping_pong_critical.shouldStop(),
           "three A-B cycles should reach ping-pong critical");
}

void testAgentLoopStopsBeforeCriticalAction() {
    const auto directory = createSkillDirectory("agent-loop-detector-workspace");
    SkillLoader loader(directory);
    ToolRegistry registry;
    expect(registry.registerFactory(
               "calculator", [] { return std::make_unique<CalculatorTool>(); }),
           "calculator registration should succeed");

    const std::string repeated_action =
        R"({"thought":"Retry","action":{"type":"tool_call","tool":"calculator","args":{"expression":"1+1"}}})";
    ScriptedClient client({successfulResponse(repeated_action),
                           successfulResponse(repeated_action),
                           successfulResponse(repeated_action)});

    AgentLoopConfig config;
    config.max_steps = 5;
    config.loop_detection.repeat = {2, 3};
    AgentLoop agent(client, registry, loader, config);
    std::vector<oop_agent::agent::AgentStep> steps;
    agent.setStepHook([&steps](const auto &step) { steps.push_back(step); });

    const auto result = agent.run("Calculate repeatedly for loop detection");
    expect(!result.success && result.steps_taken == 3 &&
               result.error_message.find("repeat loop detected") !=
                   std::string::npos,
           "AgentLoop should stop when repeat severity becomes critical");
    expect(steps.size() == 3 && steps[0].result.success &&
               steps[1].result.success && !steps[2].result.success,
           "critical repeated action should be recorded but not executed");

    std::filesystem::remove_all(directory);
}

void testMessageQueueAndMultiAgentCommunication() {
    MessageQueue<int> queue;
    expect(queue.push(10) && queue.push(20),
           "open message queue should accept messages");
    expect(queue.waitPop() == 10 && queue.waitPop() == 20,
           "message queue should preserve FIFO order");
    queue.close();
    expect(!queue.push(30) && !queue.waitPop().has_value(),
           "closed empty queue should reject pushes and unblock receivers");

    const auto first_directory =
        createSkillDirectory("multi-agent-first-workspace");
    const auto second_directory =
        createSkillDirectory("multi-agent-second-workspace");
    SkillLoader first_loader(first_directory);
    SkillLoader second_loader(second_directory);
    ToolRegistry first_registry;
    ToolRegistry second_registry;
    ScriptedClient first_client({successfulResponse(
        R"({"thought":"First done","action":{"type":"final","answer":"result-one"}})")});
    ScriptedClient second_client({successfulResponse(
        R"({"thought":"Second done","action":{"type":"final","answer":"result-two"}})")});
    AgentLoop first_agent(first_client, first_registry, first_loader);
    AgentLoop second_agent(second_client, second_registry, second_loader);
    MultiAgentCoordinator coordinator(first_agent, second_agent);

    const auto result = coordinator.run(
        ParallelAgentTask{"Solve first independent part",
                          "Solve second independent part"});
    expect(result.success(),
           "two successful agents should exchange messages successfully");
    expect(result.first.peer_message->sender_id == "agent_2" &&
               result.first.peer_message->recipient_id == "agent_1" &&
               result.first.peer_message->content == "result-two",
           "first agent should receive the second agent result");
    expect(result.second.peer_message->sender_id == "agent_1" &&
               result.second.peer_message->recipient_id == "agent_2" &&
               result.second.peer_message->content == "result-one",
           "second agent should receive the first agent result");

    std::filesystem::remove_all(first_directory);
    std::filesystem::remove_all(second_directory);
}

} // namespace

int main() {
    try {
        testSkillLoadingAndVietnameseKeywordSelection();
        testReActToolCallThenFinalAnswer();
        testMalformedActionCanSelfCorrect();
        testMaximumStepLimitIsGraceful();
        testLoopDetectorRepeatAndPingPong();
        testAgentLoopStopsBeforeCriticalAction();
        testMessageQueueAndMultiAgentCommunication();
        std::cout << "All agent core tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
