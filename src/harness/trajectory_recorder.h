#pragma once

#include "trajectory.h"

#include <filesystem>

namespace oop_agent::harness {

class TrajectoryRecorder {
  public:
    explicit TrajectoryRecorder(
        std::filesystem::path output_directory
    );

    void save(
        const Trajectory &trajectory
    ) const;

  private:
    std::filesystem::path output_directory_;
};

} // namespace oop_agent::harness
