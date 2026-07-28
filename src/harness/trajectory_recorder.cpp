#include "trajectory_recorder.h"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace oop_agent::harness {

TrajectoryRecorder::TrajectoryRecorder(
    std::filesystem::path output_directory
)
    : output_directory_(
          std::move(output_directory)
      ) {
    std::filesystem::create_directories(
        output_directory_
    );
}

void TrajectoryRecorder::save(
    const Trajectory &trajectory
) const {
    std::filesystem::create_directories(
        output_directory_
    );

    const std::filesystem::path filePath =
        output_directory_
        / (
            "trajectory_"
            + trajectory.taskId()
            + ".json"
        );

    std::ofstream outputFile(filePath);

    if (!outputFile.is_open()) {
        throw std::runtime_error(
            "Cannot create trajectory file: "
            + filePath.string()
        );
    }

    outputFile
        << trajectory.toJson().dump(4)
        << '\n';

    if (!outputFile.good()) {
        throw std::runtime_error(
            "Failed to write trajectory file: "
            + filePath.string()
        );
    }
}

} // namespace oop_agent::harness
