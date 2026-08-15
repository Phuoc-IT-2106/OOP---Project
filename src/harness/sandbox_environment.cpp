#include "sandbox_environment.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace oop_agent::harness {

SandboxEnvironment::SandboxEnvironment(
    std::filesystem::path working_directory,
    std::vector<std::filesystem::path> seed_directories,
    bool remove_on_cleanup)
    : working_directory_(
          std::filesystem::absolute(std::move(working_directory))
              .lexically_normal()),
      seed_directories_(std::move(seed_directories)),
      remove_on_cleanup_(remove_on_cleanup) {
    validateSafeDirectory(working_directory_);
    for (const auto &path : seed_directories_) {
        validateRelativePath(path);
    }
}

void SandboxEnvironment::setup() {
    std::error_code error;
    std::filesystem::remove_all(working_directory_, error);
    if (error) {
        throw std::runtime_error("cannot reset sandbox workspace: " +
                                 error.message());
    }

    std::filesystem::create_directories(working_directory_, error);
    if (error) {
        throw std::runtime_error("cannot create sandbox workspace: " +
                                 error.message());
    }

    for (const auto &directory : seed_directories_) {
        std::filesystem::create_directories(working_directory_ / directory,
                                            error);
        if (error) {
            throw std::runtime_error("cannot create sandbox directory '" +
                                     directory.string() + "': " +
                                     error.message());
        }
    }
}

void SandboxEnvironment::cleanup() {
    if (!remove_on_cleanup_) {
        return;
    }

    std::error_code error;
    std::filesystem::remove_all(working_directory_, error);
    if (error) {
        throw std::runtime_error("cannot remove sandbox workspace: " +
                                 error.message());
    }
}

const std::filesystem::path &SandboxEnvironment::workingDirectory() const
    noexcept {
    return working_directory_;
}

void SandboxEnvironment::validateSafeDirectory(
    const std::filesystem::path &path) {
    if (path.empty()) {
        throw std::invalid_argument("sandbox workspace must not be empty");
    }
    const auto normalized = path.lexically_normal();
    if (normalized == normalized.root_path()) {
        throw std::invalid_argument("sandbox workspace must not be filesystem root");
    }
    const auto current_directory =
        std::filesystem::current_path().lexically_normal();
    const auto [sandbox_end, current_match_end] =
        std::mismatch(normalized.begin(),
                      normalized.end(),
                      current_directory.begin(),
                      current_directory.end());
    const bool sandbox_contains_current =
        sandbox_end == normalized.end() &&
        current_match_end != current_directory.end();
    if (normalized == current_directory || sandbox_contains_current) {
        throw std::invalid_argument(
            "sandbox workspace must not be or contain the current project directory");
    }
}

void SandboxEnvironment::validateRelativePath(
    const std::filesystem::path &path) {
    if (path.empty() || path.is_absolute()) {
        throw std::invalid_argument(
            "sandbox seed directory must be a non-empty relative path");
    }
    for (const auto &part : path) {
        if (part == "..") {
            throw std::invalid_argument(
                "sandbox seed directory must stay inside workspace");
        }
    }
}

} // namespace oop_agent::harness
