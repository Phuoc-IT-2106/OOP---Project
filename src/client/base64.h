#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace oop_agent::client {

class Base64 {
  public:
    // Encode dữ liệu nhị phân thành Base64.
    static std::string encode(
        const std::vector<unsigned char> &data
    );

    // Đọc một file nhị phân (PNG/JPG...) và encode thành Base64.
    static std::string encodeFile(
        const std::filesystem::path &file_path
    );
};

} // namespace oop_agent::client
