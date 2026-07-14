#include "tool_registry.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace oop_agent::tools {

bool ToolRegistry::registerFactory(std::string name,
                                   ToolFactory factory,
                                   bool replace_existing) {
    if (name.empty()) {
        throw std::invalid_argument("tool name must not be empty");
    }
    if (!factory) {
        throw std::invalid_argument("tool factory must not be empty");
    }

    // Build one prototype now to catch a wrong name or null factory during
    // application setup instead of much later during an agent run.
    auto prototype = factory();
    if (!prototype) {
        throw std::invalid_argument("tool factory returned nullptr");
    }
    if (prototype->name() != name) {
        throw std::invalid_argument("registered name does not match Tool::name()");
    }

    const auto existing = entries_.find(name);
    if (existing != entries_.end() && !replace_existing) {
        return false;
    }

    Entry entry{std::string(prototype->description()), std::move(factory)};
    entries_.insert_or_assign(std::move(name), std::move(entry));
    return true;
}

bool ToolRegistry::unregisterTool(std::string_view name) {
    return entries_.erase(std::string(name)) != 0;
}

bool ToolRegistry::contains(std::string_view name) const {
    return entries_.find(std::string(name)) != entries_.end();
}

bool ToolRegistry::isAllowed(std::string_view name) const {
    const std::string key(name);
    if (deny_list_.find(key) != deny_list_.end()) {
        return false;
    }
    return allow_list_.empty() || allow_list_.find(key) != allow_list_.end();
}

std::unique_ptr<Tool> ToolRegistry::create(std::string_view name) const {
    const std::string key(name);
    const auto entry = entries_.find(key);
    if (entry == entries_.end() || !isAllowed(key)) {
        return nullptr;
    }
    return entry->second.factory();
}

ToolResult ToolRegistry::execute(std::string_view name,
                                 const ToolArguments &arguments) const {
    if (!contains(name)) {
        return ToolResult::failed("tool is not registered: " + std::string(name));
    }
    if (!isAllowed(name)) {
        return ToolResult::failed("tool is blocked by registry policy: " + std::string(name));
    }

    try {
        auto tool = create(name);
        if (!tool) {
            return ToolResult::failed("tool factory returned nullptr: " + std::string(name));
        }
        return tool->execute(arguments);
    } catch (const std::exception &error) {
        return ToolResult::failed("tool execution failed: " + std::string(error.what()));
    } catch (...) {
        return ToolResult::failed("tool execution failed with an unknown error");
    }
}

std::vector<std::string> ToolRegistry::registeredNames() const {
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (const auto &[name, entry] : entries_) {
        (void)entry;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<ToolDescriptor> ToolRegistry::availableTools() const {
    std::vector<ToolDescriptor> tools;
    tools.reserve(entries_.size());
    for (const auto &[name, entry] : entries_) {
        if (isAllowed(name)) {
            tools.push_back({name, entry.description});
        }
    }
    std::sort(tools.begin(), tools.end(), [](const auto &left, const auto &right) {
        return left.name < right.name;
    });
    return tools;
}

void ToolRegistry::allowOnly(std::vector<std::string> names) {
    allow_list_.clear();
    for (auto &name : names) {
        if (!name.empty()) {
            allow_list_.insert(std::move(name));
        }
    }
}

void ToolRegistry::deny(std::vector<std::string> names) {
    deny_list_.clear();
    for (auto &name : names) {
        if (!name.empty()) {
            deny_list_.insert(std::move(name));
        }
    }
}

void ToolRegistry::clearPolicy() {
    allow_list_.clear();
    deny_list_.clear();
}

} // namespace oop_agent::tools
