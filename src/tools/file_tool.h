#pragma once

#include "tool.h"

#include <cstddef>
#include <filesystem>

namespace oop_agent::tools {

struct FileToolConfig {
    // Relative paths are resolved under this root. Restricting access prevents
    // an LLM-generated "../" path from escaping the agent workspace.
    std::filesystem::path root_directory{"."};
    std::size_t max_read_bytes{1024 * 1024};
    bool restrict_to_root{true};
    bool create_parent_directories{true};
};

class ReadFileTool final : public Tool {
  public:
    explicit ReadFileTool(FileToolConfig config = {});

    std::string_view name() const noexcept override;
    std::string_view description() const noexcept override;
    ToolResult execute(const ToolArguments &arguments) override;

  private:
    FileToolConfig config_;
};

class WriteFileTool final : public Tool {
  public:
    explicit WriteFileTool(FileToolConfig config = {});

    std::string_view name() const noexcept override;
    std::string_view description() const noexcept override;
    ToolResult execute(const ToolArguments &arguments) override;

  private:
    FileToolConfig config_;
};

} // namespace oop_agent::tools
