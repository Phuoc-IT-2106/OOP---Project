// Placeholder: benchmark runner entrypoint.
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

int main() {
    std::ifstream file("benchmark/tasks.json");

    if (!file.is_open()) {
        std::cerr << "Cannot open benchmark/tasks.json\n";
        return 1;
    }

    nlohmann::json tasks;
    file >> tasks;

    std::cout << "Loaded "
              << tasks.size()
              << " benchmark tasks.\n\n";

    for (const auto &task : tasks) {
        std::cout
            << task["id"]
            << " : "
            << task["description"]
            << '\n';
    }

    return 0;
}
