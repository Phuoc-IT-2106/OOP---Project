#pragma once

#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace oop_agent::harness {

class Environment {
  public:
    virtual ~Environment() = default;

    virtual void setup() = 0;
    virtual void cleanup() = 0;
    virtual const std::filesystem::path &workingDirectory() const noexcept = 0;
};

// NativeEnvironment represents a normal filesystem workspace. It deliberately
// does not delete artifacts during cleanup because evaluators need to inspect
// them after AgentLoop finishes.
class NativeEnvironment final : public Environment {
  public:
    explicit NativeEnvironment(
        std::filesystem::path working_directory,
        std::vector<std::filesystem::path> reset_paths = {})
        : working_directory_(std::move(working_directory)),
          reset_paths_(std::move(reset_paths)) {
        for (const auto &path : reset_paths_) {
            if (path.is_absolute()) {
                throw std::invalid_argument(
                    "environment reset path must be relative");
            }
            for (const auto &part : path) {
                if (part == "..") {
                    throw std::invalid_argument(
                        "environment reset path must stay in workspace");
                }
            }
        }
    }

    void setup() override {
        std::filesystem::create_directories(working_directory_);
        for (const auto &path : reset_paths_) {
            std::error_code error;
            std::filesystem::remove_all(working_directory_ / path, error);
            if (error) {
                throw std::runtime_error(
                    "cannot reset benchmark artifact '" + path.string() +
                    "': " + error.message());
            }
        }
    }

    void cleanup() override {}

    const std::filesystem::path &workingDirectory() const noexcept override {
        return working_directory_;
    }

  private:
    std::filesystem::path working_directory_;
    std::vector<std::filesystem::path> reset_paths_;
};

} // namespace oop_agent::harness
