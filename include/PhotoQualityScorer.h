#pragma once

#include <string>
#include <vector>

struct PhotoQualityScore {
    double sharpness_score = 0.0;
    double exposure_score = 0.0;
    double resolution_score = 0.0;
    double contrast_score = 0.0;
    double noise_score = 0.0;
    double quality_score = 0.0;
    std::vector<std::string> reasons;
};

class PhotoQualityScorer {
public:
    static PhotoQualityScore score(const std::wstring& image_path);
};
