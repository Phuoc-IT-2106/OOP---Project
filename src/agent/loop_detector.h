#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace oop_agent::agent {

enum class LoopKind {
    None,
    Repeat,
    PingPong
};

enum class LoopSeverity {
    None,
    Warning,
    Critical
};

struct LoopThreshold {
    std::size_t warning{3};
    std::size_t critical{4};
};

struct LoopDetectorConfig {
    LoopThreshold repeat{};
    LoopThreshold ping_pong{2, 3};
    std::size_t max_history{64};
};

struct LoopDetection {
    LoopKind kind{LoopKind::None};
    LoopSeverity severity{LoopSeverity::None};
    std::size_t occurrences{0};
    std::string message;

    bool detected() const noexcept;
    bool shouldStop() const noexcept;
};

std::string_view toString(LoopKind kind) noexcept;
std::string_view toString(LoopSeverity severity) noexcept;

class LoopDetector {
  public:
    explicit LoopDetector(LoopDetectorConfig config = {});

    LoopDetection observe(std::string action_signature);
    void reset() noexcept;

    const LoopDetectorConfig &config() const noexcept;
    std::size_t historySize() const noexcept;

  private:
    static void validateThreshold(const LoopThreshold &threshold,
                                  std::string_view name);
    static LoopSeverity severityFor(std::size_t occurrences,
                                    const LoopThreshold &threshold) noexcept;
    LoopDetection makeDetection(LoopKind kind,
                                LoopSeverity severity,
                                std::size_t occurrences) const;

    LoopDetectorConfig config_;
    std::vector<std::string> history_;
};

} // namespace oop_agent::agent
