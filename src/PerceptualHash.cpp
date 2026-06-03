#include "PerceptualHash.h"

#include "PhotoImage.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

uint8_t luma(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
}

uint64_t parse_hex64(const std::string& value) {
    uint64_t parsed = 0;
    std::istringstream in(value);
    in >> std::hex >> parsed;
    if (!in) {
        throw std::runtime_error("Invalid hash hex");
    }
    return parsed;
}

} // namespace

uint64_t PerceptualHash::dhash(const std::wstring& image_path) {
    RgbImage image = load_rgb_image(image_path, 9, 8);
    uint64_t hash = 0;
    int bit = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            size_t left_offset = (static_cast<size_t>(y) * 9 + x) * 3;
            size_t right_offset = (static_cast<size_t>(y) * 9 + x + 1) * 3;
            uint8_t left = luma(
                image.rgb[left_offset + 0],
                image.rgb[left_offset + 1],
                image.rgb[left_offset + 2]);
            uint8_t right = luma(
                image.rgb[right_offset + 0],
                image.rgb[right_offset + 1],
                image.rgb[right_offset + 2]);
            if (left > right) {
                hash |= (uint64_t{1} << bit);
            }
            ++bit;
        }
    }
    return hash;
}

std::string PerceptualHash::toHex(uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

int PerceptualHash::hammingDistance(uint64_t left, uint64_t right) {
    uint64_t value = left ^ right;
    int count = 0;
    while (value != 0) {
        value &= value - 1;
        ++count;
    }
    return count;
}

int PerceptualHash::hammingDistanceHex(const std::string& left, const std::string& right) {
    if (left.empty() || right.empty()) {
        return -1;
    }
    return hammingDistance(parse_hex64(left), parse_hex64(right));
}
