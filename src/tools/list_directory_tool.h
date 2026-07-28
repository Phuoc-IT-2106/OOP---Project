#pragma once

#include "tool.h"

#include <cstddef>
#include <filesystem>

namespace oop_agent::tools {

struct ListDirectoryConfig {
    std::filesystem::path root_directory{"."};
    std::size_t default_max_entries{100};
    std::size_t max_entries_limit{500};
};

class ListDirectoryTool final : public Tool {
  public:
    explicit ListDirectoryTool(ListDirectoryConfig config = {});

    std::string_view name() const noexcept override;
    std::string_view description() const noexcept override;
    ToolResult execute(const ToolArguments &arguments) override;

  private:
    ListDirectoryConfig config_;
};

} // namespace oop_agent::tools
