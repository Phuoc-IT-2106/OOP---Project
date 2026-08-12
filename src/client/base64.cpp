#include "base64.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace oop_agent::client {

std::string Base64::encode(
    const std::vector<unsigned char> &data
) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string result;

    result.reserve(
        ((data.size() + 2) / 3) * 4
    );

    std::size_t index = 0;

    while (index + 2 < data.size()) {
        const unsigned int value =
            (static_cast<unsigned int>(data[index]) << 16) |
            (static_cast<unsigned int>(data[index + 1]) << 8) |
            static_cast<unsigned int>(data[index + 2]);

        result.push_back(table[(value >> 18) & 0x3F]);
        result.push_back(table[(value >> 12) & 0x3F]);
        result.push_back(table[(value >> 6) & 0x3F]);
        result.push_back(table[value & 0x3F]);

        index += 3;
    }

    const std::size_t remaining =
        data.size() - index;

    if (remaining == 1) {
        const unsigned int value =
            static_cast<unsigned int>(data[index]) << 16;

        result.push_back(table[(value >> 18) & 0x3F]);
        result.push_back(table[(value >> 12) & 0x3F]);
        result.push_back('=');
        result.push_back('=');
    } else if (remaining == 2) {
        const unsigned int value =
            (static_cast<unsigned int>(data[index]) << 16) |
            (static_cast<unsigned int>(data[index + 1]) << 8);

        result.push_back(table[(value >> 18) & 0x3F]);
        result.push_back(table[(value >> 12) & 0x3F]);
        result.push_back(table[(value >> 6) & 0x3F]);
        result.push_back('=');
    }

    return result;
}

std::string Base64::encodeFile(
    const std::filesystem::path &file_path
) {
    std::ifstream file(
        file_path,
        std::ios::binary
    );

    if (!file) {
        throw std::runtime_error(
            "Cannot open image file: " +
            file_path.string()
        );
    }

    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    if (data.empty()) {
        throw std::runtime_error(
            "Image file is empty: " +
            file_path.string()
        );
    }

    return encode(data);
}

} // namespace oop_agent::client
