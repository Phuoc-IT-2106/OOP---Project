#include "agent_loop.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace oop_agent::agent {
namespace {

using Json = nlohmann::json;

bool isBlank(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

std::int64_t saturatingAdd(std::int64_t left, std::int64_t right) {
    if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
        return std::numeric_limits<std::int64_t>::min();
    }
    return left + right;
}

std::string jsonValueToArgument(const Json &value) {
    return value.is_string() ? value.get<std::string>() : value.dump();
}

bool tryParseObject(std::string_view text, Json &parsed) {
    try {
        parsed = Json::parse(text);
        return parsed.is_object();
    } catch (const Json::exception &) {
        return false;
    }
}

bool findJsonObject(std::string_view text, Json &parsed) {
    if (tryParseObject(text, parsed)) {
        return true;
    }

    for (std::size_t start = text.find('{'); start != std::string_view::npos;
         start = text.find('{', start + 1)) {
        std::size_t depth = 0;
        bool in_string = false;
        bool escaped = false;

        for (std::size_t index = start; index < text.size(); ++index) {
            const char character = text[index];
            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (character == '\\') {
                    escaped = true;
                } else if (character == '"') {
                    in_string = false;
                }
                continue;
            }

            if (character == '"') {
                in_string = true;
            } else if (character == '{') {
                ++depth;
            } else if (character == '}') {
                if (depth == 0) {
                    break;
                }
                --depth;
                if (depth == 0 &&
                    tryParseObject(text.substr(start, index - start + 1), parsed)) {
                    return true;
                }
            }
        }
    }
    return false;
}

AgentRunResult failureResult(std::string message,
                             std::size_t steps_taken,
                             std::int64_t total_tokens,
                             std::int64_t total_latency,
                             std::vector<std::string> selected_skills,
                             std::vector<client::ChatMessage> conversation) {
    AgentRunResult result;
    result.error_message = std::move(message);
    result.steps_taken = steps_taken;
    result.total_tokens = total_tokens;
    result.total_latency_ms = total_latency;
    result.selected_skills = std::move(selected_skills);
    result.conversation = std::move(conversation);
    return result;
}

} // namespace

AgentLoop::AgentLoop(client::LLMClient &client,
                     tools::ToolRegistry &tool_registry,
                     skills::SkillLoader &skill_loader,
                     AgentLoopConfig config)
    : client_(client),
      tool_registry_(tool_registry),
      skill_loader_(skill_loader),
      config_(std::move(config)) {
    if (config_.max_steps == 0) {
        throw std::invalid_argument("AgentLoop max_steps must be greater than zero");
    }
    if (isBlank(config_.base_instruction)) {
        throw std::invalid_argument("AgentLoop base instruction must not be empty");
    }
}

void AgentLoop::setStepHook(StepHook hook) {
    step_hook_ = std::move(hook);
}

void AgentLoop::setMaxSteps(std::size_t max_steps) {
    if (max_steps == 0) {
        throw std::invalid_argument("AgentLoop max_steps must be greater than zero");
    }
    config_.max_steps = max_steps;
}

AgentRunResult AgentLoop::run(const std::string &task) {
    if (isBlank(task)) {
        return failureResult("task must not be empty", 0, 0, 0, {}, {});
    }

    std::vector<skills::Skill> selected;
    try {
        skill_loader_.loadSkills();
        selected = skill_loader_.selectSkills(task, config_.max_skills);
    } catch (const std::exception &error) {
        return failureResult("could not load skills: " + std::string(error.what()),
                             0, 0, 0, {}, {});
    }

    std::vector<std::string> selected_names;
    selected_names.reserve(selected.size());
    for (const auto &skill : selected) {
        selected_names.push_back(skill.name);
    }

    std::vector<client::ChatMessage> conversation{
        {"system", buildSystemPrompt(selected)},
        {"user", task},
    };

    std::int64_t total_tokens = 0;
    std::int64_t total_latency = 0;
    std::string last_error;

    for (std::size_t step_id = 0; step_id < config_.max_steps; ++step_id) {
        client::ChatResponse response;
        try {
            response = think({conversation});
        } catch (const std::exception &error) {
            return failureResult("LLM client threw an exception: " +
                                     std::string(error.what()),
                                 step_id + 1, total_tokens, total_latency,
                                 std::move(selected_names), std::move(conversation));
        } catch (...) {
            return failureResult("LLM client threw an unknown exception",
                                 step_id + 1, total_tokens, total_latency,
                                 std::move(selected_names), std::move(conversation));
        }

        const auto prompt_tokens = std::max<std::int64_t>(0, response.prompt_tokens);
        const auto completion_tokens =
            std::max<std::int64_t>(0, response.completion_tokens);
        const auto response_tokens =
            saturatingAdd(prompt_tokens, completion_tokens);
        total_tokens = saturatingAdd(total_tokens, response_tokens);
        total_latency =
            saturatingAdd(total_latency, std::max<long>(0, response.latency_ms));

        if (!response.success) {
            const std::string message = response.error_message.empty()
                                            ? "LLM request failed"
                                            : response.error_message;
            return failureResult(message, step_id + 1, total_tokens, total_latency,
                                 std::move(selected_names), std::move(conversation));
        }
        if (response.content.empty()) {
            return failureResult("LLM returned an empty response", step_id + 1,
                                 total_tokens, total_latency,
                                 std::move(selected_names), std::move(conversation));
        }

        conversation.push_back({"assistant", response.content});
        const auto parsed = parseResponse(response.content);

        AgentStep step;
        step.step_id = step_id;
        step.thought = parsed.thought;
        step.action = parsed.action;
        step.raw_response = response.content;
        step.latency_ms = response.latency_ms;
        step.tokens_used = response_tokens;

        if (!parsed.success) {
            step.result = tools::ToolResult::failed(parsed.error_message);
            last_error = parsed.error_message;

            std::string hook_error;
            if (!notifyStep(step, hook_error)) {
                return failureResult(std::move(hook_error), step_id + 1,
                                     total_tokens, total_latency,
                                     std::move(selected_names),
                                     std::move(conversation));
            }

            const Json correction = {
                {"type", "invalid_action"},
                {"error", parsed.error_message},
                {"instruction",
                 "Return exactly one valid JSON action using the required schema."},
            };
            conversation.push_back({"user", "Observation: " + correction.dump()});
            continue;
        }

        if (parsed.action.type == ActionType::FinalAnswer) {
            step.result = tools::ToolResult::ok(parsed.action.final_answer);

            std::string hook_error;
            if (!notifyStep(step, hook_error)) {
                return failureResult(std::move(hook_error), step_id + 1,
                                     total_tokens, total_latency,
                                     std::move(selected_names),
                                     std::move(conversation));
            }

            AgentRunResult result;
            result.success = true;
            result.final_answer = parsed.action.final_answer;
            result.steps_taken = step_id + 1;
            result.total_tokens = total_tokens;
            result.total_latency_ms = total_latency;
            result.selected_skills = std::move(selected_names);
            result.conversation = std::move(conversation);
            return result;
        }

        try {
            step.result = act(parsed.action);
        } catch (const std::exception &error) {
            step.result =
                tools::ToolResult::failed("tool execution threw an exception: " +
                                          std::string(error.what()));
        } catch (...) {
            step.result =
                tools::ToolResult::failed("tool execution threw an unknown exception");
        }

        last_error = step.result.success ? std::string{} : step.result.error_message;

        std::string hook_error;
        if (!notifyStep(step, hook_error)) {
            return failureResult(std::move(hook_error), step_id + 1,
                                 total_tokens, total_latency,
                                 std::move(selected_names),
                                 std::move(conversation));
        }

        conversation.push_back({"user", "Observation: " + observe(step.result)});
    }

    std::string message = "maximum step limit reached after " +
                          std::to_string(config_.max_steps) + " steps";
    if (!last_error.empty()) {
        message += "; last error: " + last_error;
    }
    return failureResult(std::move(message), config_.max_steps, total_tokens,
                         total_latency, std::move(selected_names),
                         std::move(conversation));
}

client::ChatResponse AgentLoop::think(const client::ChatRequest &request) {
    return client_.chat(request);
}

tools::ToolResult AgentLoop::act(const AgentAction &action) {
    if (action.type != ActionType::ToolCall) {
        return tools::ToolResult::failed("action is not a tool call");
    }
    return tool_registry_.execute(action.tool_name, action.arguments);
}

std::string AgentLoop::observe(const tools::ToolResult &result) const {
    Json observation = {
        {"type", "tool_result"},
        {"success", result.success},
        {"output", result.output},
        {"error", result.error_message},
    };
    if (result.exit_code.has_value()) {
        observation["exit_code"] = *result.exit_code;
    }
    return observation.dump();
}

AgentLoop::ParsedResponse AgentLoop::parseResponse(const std::string &content) const {
    ParsedResponse response;
    Json root;
    if (!findJsonObject(content, root)) {
        response.error_message = "malformed JSON action";
        return response;
    }

    const auto thought = root.find("thought");
    if (thought == root.end() || !thought->is_string()) {
        response.error_message = "missing string field 'thought'";
        return response;
    }
    response.thought = thought->get<std::string>();

    const auto action = root.find("action");
    if (action == root.end() || !action->is_object()) {
        response.error_message = "missing object field 'action'";
        return response;
    }

    const auto type = action->find("type");
    if (type == action->end() || !type->is_string()) {
        response.error_message = "missing string field 'action.type'";
        return response;
    }

    const auto type_name = type->get<std::string>();
    if (type_name == "final" || type_name == "final_answer") {
        const auto answer = action->find("answer");
        if (answer == action->end() || !answer->is_string() ||
            isBlank(answer->get_ref<const std::string &>())) {
            response.error_message =
                "final action requires a non-empty string field 'answer'";
            return response;
        }
        response.action.type = ActionType::FinalAnswer;
        response.action.final_answer = answer->get<std::string>();
        response.success = true;
        return response;
    }

    if (type_name != "tool_call") {
        response.error_message = "unsupported action type: " + type_name;
        return response;
    }

    const auto tool = action->find("tool");
    if (tool == action->end() || !tool->is_string() ||
        isBlank(tool->get_ref<const std::string &>())) {
        response.error_message =
            "tool_call requires a non-empty string field 'tool'";
        return response;
    }

    const auto arguments = action->find("args");
    if (arguments == action->end() || !arguments->is_object()) {
        response.error_message = "tool_call requires an object field 'args'";
        return response;
    }

    response.action.type = ActionType::ToolCall;
    response.action.tool_name = tool->get<std::string>();
    for (auto iterator = arguments->begin(); iterator != arguments->end(); ++iterator) {
        response.action.arguments.emplace(iterator.key(),
                                          jsonValueToArgument(iterator.value()));
    }
    response.success = true;
    return response;
}

std::string AgentLoop::buildSystemPrompt(
    const std::vector<skills::Skill> &selected) const {
    std::ostringstream prompt;
    prompt << config_.base_instruction << "\n\n"
           << "Use this ReAct cycle: Observe -> Think -> Act -> Observe.\n"
           << "At each step return exactly one JSON object, without markdown fences.\n"
           << "To call a tool:\n"
           << R"({"thought":"brief reasoning","action":{"type":"tool_call","tool":"tool_name","args":{"argument":"value"}}})"
           << "\nTo finish:\n"
           << R"({"thought":"brief reasoning","action":{"type":"final","answer":"final response"}})"
           << "\nAll tool arguments must be a JSON object. Use the exact argument "
              "names from each tool description.\n\n"
           << "Available tools:\n";

    const auto available_tools = tool_registry_.availableTools();
    if (available_tools.empty()) {
        prompt << "- No tools are currently available.\n";
    } else {
        for (const auto &tool : available_tools) {
            prompt << "- " << tool.name << ": " << tool.description << '\n';
        }
    }

    if (!selected.empty()) {
        prompt << "\nSelected skills:\n";
        for (const auto &skill : selected) {
            prompt << "\n[" << skill.name << "]\n"
                   << skill.instructions << '\n';
        }
    }
    return prompt.str();
}

bool AgentLoop::notifyStep(const AgentStep &step,
                           std::string &error_message) const {
    if (!step_hook_) {
        return true;
    }
    try {
        step_hook_(step);
        return true;
    } catch (const std::exception &error) {
        error_message = "step hook failed: " + std::string(error.what());
    } catch (...) {
        error_message = "step hook failed with an unknown exception";
    }
    return false;
}

} // namespace oop_agent::agent
