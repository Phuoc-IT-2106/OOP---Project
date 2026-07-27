// Placeholder: TrajectoryRecorder declaration for JSON output.
#ifndef TRAJECTORY_RECORDER_H
#define TRAJECTORY_RECORDER_H

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "trajectory.h"

class TrajectoryRecorder {
private:
    std::filesystem::path outputDirectory;

public:
    explicit TrajectoryRecorder(
        const std::filesystem::path& outputDirectory
    )
        : outputDirectory(outputDirectory) {
        std::filesystem::create_directories(
            outputDirectory
        );
    }

    void save(const Trajectory& trajectory) const {
        const std::filesystem::path outputPath =
            outputDirectory
            / (
                "trajectory_"
                + trajectory.getTaskId()
                + ".json"
            );

        std::ofstream outputFile(outputPath);

        if (!outputFile.is_open()) {
            throw std::runtime_error(
                "Khong the tao file trajectory: "
                + outputPath.string()
            );
        }

        outputFile << trajectory.toJson().dump(4);
    }
};

#endif
