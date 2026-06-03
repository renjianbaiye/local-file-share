#pragma once

#include <cstdint>
#include <string>

class PerceptualHash {
public:
    static uint64_t dhash(const std::wstring& image_path);
    static std::string toHex(uint64_t value);
    static int hammingDistance(uint64_t left, uint64_t right);
    static int hammingDistanceHex(const std::string& left, const std::string& right);
};
