#include "loop_detector.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace oop_agent::agent {

bool LoopDetection::detected() const noexcept {
    return severity != LoopSeverity::None;
}

bool LoopDetection::shouldStop() const noexcept {
    return severity == LoopSeverity::Critical;
}

std::string_view toString(LoopKind kind) noexcept {
    switch (kind) {
    case LoopKind::Repeat:
        return "repeat";
    case LoopKind::PingPong:
        return "ping-pong";
    case LoopKind::None:
    default:
        return "none";
    }
}

std::string_view toString(LoopSeverity severity) noexcept {
    switch (severity) {
    case LoopSeverity::Warning:
        return "warning";
    case LoopSeverity::Critical:
        return "critical";
    case LoopSeverity::None:
    default:
        return "none";
    }
}

LoopDetector::LoopDetector(LoopDetectorConfig config)
    : config_(std::move(config)) {
    validateThreshold(config_.repeat, "repeat");
    validateThreshold(config_.ping_pong, "ping-pong");

    if (config_.ping_pong.critical >
        std::numeric_limits<std::size_t>::max() / 2) {
        throw std::invalid_argument(
            "ping-pong critical threshold is too large");
    }
    const auto required_history =
        std::max(config_.repeat.critical, config_.ping_pong.critical * 2);
    if (config_.max_history < required_history) {
        throw std::invalid_argument(
            "loop detector max_history is smaller than a critical threshold");
    }
    history_.reserve(config_.max_history);
}

LoopDetection LoopDetector::observe(std::string action_signature) {
    if (action_signature.empty()) {
        throw std::invalid_argument("action signature must not be empty");
    }

    if (history_.size() == config_.max_history) {
        history_.erase(history_.begin());
    }
    history_.push_back(std::move(action_signature));

    std::size_t repeat_count = 1;
    for (std::size_t index = history_.size(); index > 1; --index) {
        if (history_[index - 1] != history_[index - 2]) {
            break;
        }
        ++repeat_count;
    }
    const auto repeat_severity = severityFor(repeat_count, config_.repeat);
    if (repeat_severity != LoopSeverity::None) {
        return makeDetection(LoopKind::Repeat, repeat_severity, repeat_count);
    }

    if (history_.size() < 4 ||
        history_[history_.size() - 1] == history_[history_.size() - 2]) {
        return {};
    }

    const auto &last = history_[history_.size() - 1];
    const auto &previous = history_[history_.size() - 2];
    std::size_t alternating_entries = 2;
    for (std::size_t distance = 2; distance < history_.size(); ++distance) {
        const auto &expected = distance % 2 == 0 ? last : previous;
        if (history_[history_.size() - 1 - distance] != expected) {
            break;
        }
        ++alternating_entries;
    }

    const std::size_t cycles = alternating_entries / 2;
    const auto ping_pong_severity =
        severityFor(cycles, config_.ping_pong);
    if (ping_pong_severity != LoopSeverity::None) {
        return makeDetection(LoopKind::PingPong, ping_pong_severity, cycles);
    }
    return {};
}

void LoopDetector::reset() noexcept {
    history_.clear();
}

const LoopDetectorConfig &LoopDetector::config() const noexcept {
    return config_;
}

std::size_t LoopDetector::historySize() const noexcept {
    return history_.size();
}

void LoopDetector::validateThreshold(const LoopThreshold &threshold,
                                     std::string_view name) {
    if (threshold.warning < 2) {
        throw std::invalid_argument(std::string(name) +
                                    " warning threshold must be at least 2");
    }
    if (threshold.critical <= threshold.warning) {
        throw std::invalid_argument(std::string(name) +
                                    " critical threshold must be greater than warning");
    }
}

LoopSeverity LoopDetector::severityFor(
    std::size_t occurrences,
    const LoopThreshold &threshold) noexcept {
    if (occurrences >= threshold.critical) {
        return LoopSeverity::Critical;
    }
    if (occurrences >= threshold.warning) {
        return LoopSeverity::Warning;
    }
    return LoopSeverity::None;
}

LoopDetection LoopDetector::makeDetection(LoopKind kind,
                                          LoopSeverity severity,
                                          std::size_t occurrences) const {
    LoopDetection detection;
    detection.kind = kind;
    detection.severity = severity;
    detection.occurrences = occurrences;
    detection.message = std::string(toString(severity)) + " " +
                        std::string(toString(kind)) + " loop detected (" +
                        std::to_string(occurrences) +
                        (kind == LoopKind::PingPong ? " cycles)" : " repeats)");
    return detection;
}

} // namespace oop_agent::agent
