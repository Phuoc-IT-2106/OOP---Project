#pragma once

#include "environment.h"

#include <filesystem>
#include <vector>

namespace oop_agent::harness {

// SandboxEnvironment provides a disposable workspace for tests, benchmarks, or
// risky agent runs. The caller chooses the root directory; setup recreates it
// and cleanup can remove it so generated artifacts do not leak into the repo.
class SandboxEnvironment final : public Environment {
  public:
    explicit SandboxEnvironment(
        std::filesystem::path working_directory,
        std::vector<std::filesystem::path> seed_directories = {},
        bool remove_on_cleanup = true);

    void setup() override;
    void cleanup() override;
    const std::filesystem::path &workingDirectory() const noexcept override;

  private:
    static void validateSafeDirectory(const std::filesystem::path &path);
    static void validateRelativePath(const std::filesystem::path &path);

    std::filesystem::path working_directory_;
    std::vector<std::filesystem::path> seed_directories_;
    bool remove_on_cleanup_{true};
};

} // namespace oop_agent::harness
