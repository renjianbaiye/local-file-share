#include "PhotoImage.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

class GdiPlusRuntime {
public:
    GdiPlusRuntime() {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&token_, &input, nullptr) != Gdiplus::Ok) {
            throw std::runtime_error("Failed to start GDI+");
        }
    }

    ~GdiPlusRuntime() {
        if (token_ != 0) {
            Gdiplus::GdiplusShutdown(token_);
        }
    }

private:
    ULONG_PTR token_ = 0;
};

GdiPlusRuntime& gdiplus_runtime() {
    static GdiPlusRuntime runtime;
    return runtime;
}

RgbImage render_rgb_image(Gdiplus::Bitmap& source, int target_width, int target_height) {
    int original_width = static_cast<int>(source.GetWidth());
    int original_height = static_cast<int>(source.GetHeight());
    int width = target_width > 0 ? target_width : original_width;
    int height = target_height > 0 ? target_height : original_height;
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid image size");
    }

    Gdiplus::Bitmap bitmap(width, height, PixelFormat24bppRGB);
    Gdiplus::Graphics graphics(&bitmap);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(&source, 0, 0, width, height);

    Gdiplus::Rect rect(0, 0, width, height);
    Gdiplus::BitmapData data = {};
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, &data) != Gdiplus::Ok) {
        throw std::runtime_error("Failed to lock image pixels");
    }

    RgbImage image;
    image.width = width;
    image.height = height;
    image.original_width = original_width;
    image.original_height = original_height;
    image.rgb.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);

    const unsigned char* base = static_cast<const unsigned char*>(data.Scan0);
    for (int y = 0; y < height; ++y) {
        const unsigned char* row = base + y * data.Stride;
        for (int x = 0; x < width; ++x) {
            const unsigned char* pixel = row + x * 3;
            size_t offset = (static_cast<size_t>(y) * width + x) * 3;
            image.rgb[offset + 0] = pixel[2];
            image.rgb[offset + 1] = pixel[1];
            image.rgb[offset + 2] = pixel[0];
        }
    }

    bitmap.UnlockBits(&data);
    return image;
}

} // namespace

RgbImage load_rgb_image(const std::wstring& image_path, int target_width, int target_height) {
    (void)gdiplus_runtime();

    Gdiplus::Bitmap source(image_path.c_str());
    if (source.GetLastStatus() != Gdiplus::Ok) {
        throw std::runtime_error("Failed to load image");
    }

    return render_rgb_image(source, target_width, target_height);
}

RgbImage load_rgb_image_max(const std::wstring& image_path, int max_side) {
    if (max_side <= 0) {
        return load_rgb_image(image_path);
    }

    (void)gdiplus_runtime();
    Gdiplus::Bitmap source(image_path.c_str());
    if (source.GetLastStatus() != Gdiplus::Ok) {
        throw std::runtime_error("Failed to load image");
    }

    int original_width = static_cast<int>(source.GetWidth());
    int original_height = static_cast<int>(source.GetHeight());
    if (original_width <= 0 || original_height <= 0) {
        throw std::runtime_error("Invalid image size");
    }

    int longest = std::max(original_width, original_height);
    if (longest <= max_side) {
        return render_rgb_image(source, 0, 0);
    }

    double scale = static_cast<double>(max_side) / static_cast<double>(longest);
    int target_width = std::max(1, static_cast<int>(std::lround(original_width * scale)));
    int target_height = std::max(1, static_cast<int>(std::lround(original_height * scale)));
    return render_rgb_image(source, target_width, target_height);
}
