#include "PhotoQualityScorer.h"

#include "PhotoImage.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {

double clamp(double value) {
    return std::max(0.0, std::min(1.0, value));
}

double gray_at(const std::vector<float>& gray, int width, int height, int x, int y) {
    x = std::max(0, std::min(width - 1, x));
    y = std::max(0, std::min(height - 1, y));
    return gray[static_cast<size_t>(y) * width + x];
}

double resolution_score(int width, int height) {
    double megapixels = (static_cast<double>(width) * height) / 1000000.0;
    if (megapixels >= 8.0) return 1.0;
    if (megapixels >= 4.0) return 0.8;
    if (megapixels >= 2.0) return 0.6;
    return 0.4;
}

} // namespace

PhotoQualityScore PhotoQualityScorer::score(const std::wstring& image_path) {
    constexpr int quality_max_side = 1024;
    RgbImage image = load_rgb_image_max(image_path, quality_max_side);
    const int width = image.width;
    const int height = image.height;
    const size_t count = static_cast<size_t>(width) * height;

    std::vector<float> gray(count);
    for (size_t i = 0; i < count; ++i) {
        uint8_t r = image.rgb[i * 3 + 0];
        uint8_t g = image.rgb[i * 3 + 1];
        uint8_t b = image.rgb[i * 3 + 2];
        gray[i] = static_cast<float>(0.299 * r + 0.587 * g + 0.114 * b);
    }

    double mean = std::accumulate(gray.begin(), gray.end(), 0.0) / std::max<size_t>(1, gray.size());
    double variance_sum = 0.0;
    int dark_count = 0;
    int bright_count = 0;
    double lap_mean = 0.0;
    double lap_square_sum = 0.0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t index = static_cast<size_t>(y) * width + x;
            double luma = gray[index] / 255.0;
            if (luma < 0.05) ++dark_count;
            if (luma > 0.95) ++bright_count;
            double diff = gray[index] - mean;
            variance_sum += diff * diff;

            double lap = -4.0 * gray[index]
                + gray_at(gray, width, height, x - 1, y)
                + gray_at(gray, width, height, x + 1, y)
                + gray_at(gray, width, height, x, y - 1)
                + gray_at(gray, width, height, x, y + 1);
            lap_mean += lap;
            lap_square_sum += lap * lap;
        }
    }
    lap_mean /= std::max<size_t>(1, count);
    double lap_var = lap_square_sum / std::max<size_t>(1, count) - lap_mean * lap_mean;
    lap_var = std::max(0.0, lap_var);

    double gray_std = std::sqrt(variance_sum / std::max<size_t>(1, count));
    double dark_ratio = static_cast<double>(dark_count) / std::max<size_t>(1, count);
    double bright_ratio = static_cast<double>(bright_count) / std::max<size_t>(1, count);
    double mean_luma = mean / 255.0;

    double delta_sum = 0.0;
    size_t delta_count = 0;
    int sample_step = std::max(1, std::max(width, height) / 512);
    for (int y = 0; y < height; y += sample_step) {
        for (int x = 0; x < width; x += sample_step) {
            double sum = 0.0;
            int n = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    sum += gray_at(gray, width, height, x + dx, y + dy);
                    ++n;
                }
            }
            double blurred = sum / n;
            delta_sum += std::abs(gray[static_cast<size_t>(y) * width + x] - blurred);
            ++delta_count;
        }
    }
    double average_delta = delta_count == 0 ? 0.0 : delta_sum / static_cast<double>(delta_count);

    PhotoQualityScore result;
    result.sharpness_score = clamp(std::log(lap_var + 1.0) / std::log(1000.0));
    result.exposure_score = clamp(
        1.0 - dark_ratio * 1.5 - bright_ratio * 1.5 - std::abs(mean_luma - 0.5) * 0.8);
    result.resolution_score = resolution_score(
        image.original_width > 0 ? image.original_width : width,
        image.original_height > 0 ? image.original_height : height);
    result.contrast_score = clamp(gray_std / 128.0);
    result.noise_score = clamp(1.0 - (average_delta / 32.0) * 0.25);
    result.quality_score = clamp(
        0.45 * result.sharpness_score
        + 0.25 * result.exposure_score
        + 0.15 * result.resolution_score
        + 0.10 * result.contrast_score
        + 0.05 * result.noise_score);

    if (result.sharpness_score < 0.35) result.reasons.push_back("low_sharpness");
    if (result.exposure_score < 0.55) result.reasons.push_back("exposure_issue");
    if (result.resolution_score < 0.6) result.reasons.push_back("low_resolution");
    if (result.contrast_score < 0.2) result.reasons.push_back("low_contrast");
    if (result.noise_score < 0.7) result.reasons.push_back("possible_noise");
    return result;
}
