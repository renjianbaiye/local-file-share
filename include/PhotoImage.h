#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct RgbImage {
    int width = 0;
    int height = 0;
    int original_width = 0;
    int original_height = 0;
    std::vector<uint8_t> rgb;
};

RgbImage load_rgb_image(const std::wstring& image_path, int target_width = 0, int target_height = 0);
RgbImage load_rgb_image_max(const std::wstring& image_path, int max_side);
