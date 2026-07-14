#pragma once

#include "tool.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace oop_agent::tools {

struct ToolDescriptor {
    std::string name;
    std::string description;
};

// Registry/Factory pattern: concrete tool types are registered at runtime as
// lambdas. Adding a new tool therefore does not require editing ToolRegistry.
class ToolRegistry {
  public:
    using ToolFactory = std::function<std::unique_ptr<Tool>()>;

    // Returns false when the name already exists and replace_existing is false.
    // Invalid factories throw std::invalid_argument so setup errors fail early.
    bool registerFactory(std::string name,
                         ToolFactory factory,
                         bool replace_existing = false);
    bool unregisterTool(std::string_view name);

    bool contains(std::string_view name) const;
    bool isAllowed(std::string_view name) const;

    // Creates a fresh instance so each agent run gets isolated tool state.
    // A missing or policy-blocked tool returns nullptr.
    std::unique_ptr<Tool> create(std::string_view name) const;
    ToolResult execute(std::string_view name, const ToolArguments &arguments) const;

    std::vector<std::string> registeredNames() const;
    std::vector<ToolDescriptor> availableTools() const;

    // Empty allow-list means "allow every registered tool". Deny-list always
    // wins, which makes emergency blocking predictable.
    void allowOnly(std::vector<std::string> names);
    void deny(std::vector<std::string> names);
    void clearPolicy();

  private:
    struct Entry {
        std::string description;
        ToolFactory factory;
    };

    std::unordered_map<std::string, Entry> entries_;
    std::unordered_set<std::string> allow_list_;
    std::unordered_set<std::string> deny_list_;
};

} // namespace oop_agent::tools
