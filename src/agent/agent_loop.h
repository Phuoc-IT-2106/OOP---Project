#pragma once

#include "client/llm_client.h"
#include "skills/skill_loader.h"
#include "tools/tool_registry.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace oop_agent::agent {

enum class ActionType {
    ToolCall,
    FinalAnswer,
    Invalid
};

struct AgentAction {
    ActionType type{ActionType::Invalid};
    std::string tool_name;
    tools::ToolArguments arguments;
    std::string final_answer;
};

struct AgentStep {
    std::size_t step_id{0};
    std::string thought;
    AgentAction action;
    tools::ToolResult result;
    std::string raw_response;
    long latency_ms{0};
    std::int64_t tokens_used{0};
};

struct AgentRunResult {
    bool success{false};
    std::string final_answer;
    std::string error_message;
    std::size_t steps_taken{0};
    std::int64_t total_tokens{0};
    std::int64_t total_latency_ms{0};
    std::vector<std::string> selected_skills;
    std::vector<client::ChatMessage> conversation;
};

struct AgentLoopConfig {
    std::size_t max_steps{10};
    std::size_t max_skills{3};
    std::string base_instruction{
        "You are an AI agent that solves the user's task with the available tools."};
};

// Observer/Hook boundary: HarnessRunner can subscribe without AgentLoop knowing
// that a harness or trajectory recorder exists.
using StepHook = std::function<void(const AgentStep &)>;

class AgentLoop {
  public:
    AgentLoop(client::LLMClient &client,
              tools::ToolRegistry &tool_registry,
              skills::SkillLoader &skill_loader,
              AgentLoopConfig config = {});
    virtual ~AgentLoop() = default;

    AgentLoop(const AgentLoop &) = delete;
    AgentLoop &operator=(const AgentLoop &) = delete;

    void setStepHook(StepHook hook);
    void setMaxSteps(std::size_t max_steps);

    // Template Method: run owns the fixed ReAct skeleton while subclasses may
    // customize the Think, Act, and Observe phases below.
    AgentRunResult run(const std::string &task);

  protected:
    virtual client::ChatResponse think(const client::ChatRequest &request);
    virtual tools::ToolResult act(const AgentAction &action);
    virtual std::string observe(const tools::ToolResult &result) const;

  private:
    struct ParsedResponse {
        bool success{false};
        std::string thought;
        AgentAction action;
        std::string error_message;
    };

    ParsedResponse parseResponse(const std::string &content) const;
    std::string buildSystemPrompt(const std::vector<skills::Skill> &selected) const;
    bool notifyStep(const AgentStep &step, std::string &error_message) const;

    client::LLMClient &client_;
    tools::ToolRegistry &tool_registry_;
    skills::SkillLoader &skill_loader_;
    AgentLoopConfig config_;
    StepHook step_hook_;
};

} // namespace oop_agent::agent
