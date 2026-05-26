#include "PhotoTagger.h"

#include "FileManager.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifdef LFS_ENABLE_ONNXRUNTIME
#include <objidl.h>
#include <gdiplus.h>
#include <onnxruntime_cxx_api.h>
#endif

namespace fs = std::filesystem;

namespace {

const int kInputSize = 224;
const float kMean[3] = {0.485f, 0.456f, 0.406f};
const float kStd[3] = {0.229f, 0.224f, 0.225f};

const char* kDefaultLabels[] = {
    "person", "portrait", "group_people", "food", "landscape", "nature",
    "water", "mountain", "beach", "sky", "building", "landmark", "city",
    "indoor", "night", "text_image", "document", "screenshot",
};

const std::map<std::string, double> kDefaultThresholds = {
    {"person", 0.6099998950958252},
    {"portrait", 0.22999995946884155},
    {"group_people", 0.5},
    {"food", 0.8999998569488525},
    {"landscape", 0.05000000074505806},
    {"nature", 0.7699998617172241},
    {"water", 0.5999999046325684},
    {"mountain", 0.09999999403953552},
    {"beach", 0.5099999308586121},
    {"sky", 0.07999999821186066},
    {"building", 0.34999996423721313},
    {"landmark", 0.5899999141693115},
    {"city", 0.31999996304512024},
    {"indoor", 0.24999995529651642},
    {"night", 0.05000000074505806},
    {"text_image", 0.49999991059303284},
    {"document", 0.21999995410442352},
    {"screenshot", 0.49999991059303284},
};

std::wstring quote_arg(const std::wstring& value) {
    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L"\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring absolute_from_root(const std::wstring& root, const std::wstring& path) {
    fs::path value(path);
    if (value.is_absolute()) {
        return value.wstring();
    }
    return (fs::path(root) / value).wstring();
}

std::wstring temp_json_path() {
    wchar_t temp_dir[MAX_PATH + 1] = {};
    DWORD length = GetTempPathW(MAX_PATH, temp_dir);
    if (length == 0 || length > MAX_PATH) {
        throw std::runtime_error("Failed to get temporary directory");
    }

    wchar_t temp_file[MAX_PATH + 1] = {};
    if (GetTempFileNameW(temp_dir, L"lfs", 0, temp_file) == 0) {
        throw std::runtime_error("Failed to create temporary file path");
    }
    DeleteFileW(temp_file);
    return std::wstring(temp_file) + L".json";
}

std::wstring default_log_path(const std::wstring& project_root) {
    return (fs::path(project_root) / L"tagger-last.log").wstring();
}

std::string read_utf8_file(const std::wstring& path) {
    std::ifstream in(fs::path(path), std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to read tagger output JSON");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> parse_string_array(const std::string& json, const std::string& key) {
    std::vector<std::string> values;
    std::regex array_regex("\"" + key + "\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
    std::smatch array_match;
    if (!std::regex_search(json, array_match, array_regex)) {
        return values;
    }

    std::string body = array_match[1].str();
    std::regex string_regex("\"((?:[^\"\\\\]|\\\\.)*)\"");
    for (std::sregex_iterator it(body.begin(), body.end(), string_regex), end; it != end; ++it) {
        values.push_back((*it)[1].str());
    }
    return values;
}

double parse_threshold(const std::string& json, const std::string& label) {
    std::regex threshold_regex("\"" + label + "\"\\s*:\\s*([-+0-9.eE]+)");
    std::smatch match;
    if (std::regex_search(json, match, threshold_regex)) {
        return std::stod(match[1].str());
    }

    auto found = kDefaultThresholds.find(label);
    return found == kDefaultThresholds.end() ? 0.5 : found->second;
}

std::wstring metadata_path_for_model(const std::wstring& model_path) {
    fs::path path(model_path);
    return (path.parent_path() / (path.stem().wstring() + L".metadata.json")).wstring();
}

struct ModelMetadata {
    std::vector<std::string> labels;
    std::map<std::string, double> thresholds;
};

ModelMetadata load_model_metadata(const std::wstring& model_path) {
    ModelMetadata metadata;
    std::wstring metadata_path = metadata_path_for_model(model_path);
    std::string json;
    if (fs::exists(fs::path(metadata_path))) {
        json = read_utf8_file(metadata_path);
        metadata.labels = parse_string_array(json, "labels");
    }

    if (metadata.labels.empty()) {
        metadata.labels.assign(std::begin(kDefaultLabels), std::end(kDefaultLabels));
    }

    for (const std::string& label : metadata.labels) {
        metadata.thresholds[label] = json.empty() ? parse_threshold("", label) : parse_threshold(json, label);
    }
    return metadata;
}

std::vector<PhotoTag> derive_tags_from_probabilities(
    const std::vector<std::string>& labels,
    const std::map<std::string, double>& thresholds,
    const std::vector<float>& probabilities) {
    std::vector<PhotoTag> tags;
    std::set<std::string> predicted;
    std::map<std::string, double> probability_by_label;

    for (size_t i = 0; i < labels.size() && i < probabilities.size(); ++i) {
        const std::string& label = labels[i];
        double threshold = 0.5;
        auto found = thresholds.find(label);
        if (found != thresholds.end()) {
            threshold = found->second;
        }

        double probability = probabilities[i];
        bool is_predicted = probability >= threshold;
        probability_by_label[label] = probability;
        if (is_predicted) {
            predicted.insert(label);
        }

        PhotoTag tag;
        tag.tag = label;
        tag.probability = probability;
        tag.threshold = threshold;
        tag.predicted = is_predicted;
        tag.derived = false;
        tags.push_back(tag);
    }

    auto add_derived = [&](const std::string& name) {
        if (predicted.insert(name).second) {
            PhotoTag tag;
            tag.tag = name;
            tag.probability = 1.0;
            tag.threshold = 1.0;
            tag.predicted = true;
            tag.derived = true;
            tags.push_back(tag);
        }
    };

    if (predicted.count("person") != 0 && predicted.count("portrait") != 0) {
        add_derived("people_photo");
    }

    const std::set<std::string> travel_signals = {
        "landscape", "nature", "water", "mountain", "beach",
        "sky", "building", "landmark", "city",
        "scenery", "city_view", "street", "architecture",
        "temple_or_historic", "sea_or_lake", "river_or_water",
        "forest", "park", "station_or_airport",
    };

    bool has_travel_signal = false;
    for (const std::string& signal : travel_signals) {
        if (predicted.count(signal) != 0) {
            has_travel_signal = true;
            break;
        }
    }
    if (has_travel_signal) {
        add_derived("travel_or_scenery");
    }
    bool has_people_photo_signal = predicted.count("people_photo") != 0 ||
        (predicted.count("person") != 0 && predicted.count("portrait") != 0);
    if (has_people_photo_signal && has_travel_signal) {
        add_derived("travel_checkin");
    }

    if (probability_by_label["person"] >= 0.80 && probability_by_label["portrait"] >= 0.60) {
        add_derived("group_people");
    }

    if (predicted.count("screenshot") != 0 || predicted.count("document") != 0 ||
        predicted.count("text_image") != 0 || predicted.count("document_or_screen") != 0) {
        add_derived("text_or_screen");
    }

    return tags;
}

std::vector<PhotoTag> parse_label_results(const std::string& json) {
    std::vector<PhotoTag> tags;
    std::regex label_regex(
        "\\{\\s*\"label\"\\s*:\\s*\"([^\"]+)\"\\s*,\\s*"
        "\"probability\"\\s*:\\s*([-+0-9.eE]+)\\s*,\\s*"
        "\"threshold\"\\s*:\\s*([-+0-9.eE]+)\\s*,\\s*"
        "\"predicted\"\\s*:\\s*(true|false)\\s*\\}");

    for (std::sregex_iterator it(json.begin(), json.end(), label_regex), end; it != end; ++it) {
        PhotoTag tag;
        tag.tag = (*it)[1].str();
        tag.probability = std::stod((*it)[2].str());
        tag.threshold = std::stod((*it)[3].str());
        tag.predicted = (*it)[4].str() == "true";
        tag.derived = false;
        tags.push_back(tag);
    }
    return tags;
}

std::vector<PhotoTag> parse_prediction_json(const std::string& json) {
    std::vector<PhotoTag> tags = parse_label_results(json);
    std::set<std::string> model_labels;
    for (const PhotoTag& tag : tags) {
        model_labels.insert(tag.tag);
    }

    std::vector<std::string> derived_tags = parse_string_array(json, "derived_tags");
    for (const std::string& derived : derived_tags) {
        if (model_labels.find(derived) != model_labels.end()) {
            continue;
        }

        PhotoTag tag;
        tag.tag = derived;
        tag.probability = 1.0;
        tag.threshold = 1.0;
        tag.predicted = true;
        tag.derived = true;
        tags.push_back(tag);
    }
    return tags;
}

} // namespace

#ifdef LFS_ENABLE_ONNXRUNTIME
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

std::vector<float> load_image_tensor(const std::wstring& image_path) {
    static GdiPlusRuntime gdiplus;

    Gdiplus::Bitmap source(image_path.c_str());
    if (source.GetLastStatus() != Gdiplus::Ok) {
        throw std::runtime_error("Failed to load image for ONNX inference");
    }

    Gdiplus::Bitmap resized(kInputSize, kInputSize, PixelFormat24bppRGB);
    Gdiplus::Graphics graphics(&resized);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(&source, 0, 0, kInputSize, kInputSize);

    Gdiplus::Rect rect(0, 0, kInputSize, kInputSize);
    Gdiplus::BitmapData data = {};
    if (resized.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, &data) != Gdiplus::Ok) {
        throw std::runtime_error("Failed to lock image pixels");
    }

    std::vector<float> tensor(3 * kInputSize * kInputSize);
    const int stride = data.Stride;
    const unsigned char* base = static_cast<const unsigned char*>(data.Scan0);
    for (int y = 0; y < kInputSize; ++y) {
        const unsigned char* row = base + y * stride;
        for (int x = 0; x < kInputSize; ++x) {
            const unsigned char* pixel = row + x * 3;
            float b = pixel[0] / 255.0f;
            float g = pixel[1] / 255.0f;
            float r = pixel[2] / 255.0f;
            int offset = y * kInputSize + x;
            tensor[offset] = (r - kMean[0]) / kStd[0];
            tensor[kInputSize * kInputSize + offset] = (g - kMean[1]) / kStd[1];
            tensor[2 * kInputSize * kInputSize + offset] = (b - kMean[2]) / kStd[2];
        }
    }

    resized.UnlockBits(&data);
    return tensor;
}

class OnnxPhotoTagger : public PhotoTagger {
public:
    explicit OnnxPhotoTagger(OnnxPhotoTaggerOptions options)
        : options_(std::move(options)),
          metadata_(load_model_metadata(options_.model_path)),
          env_(ORT_LOGGING_LEVEL_ERROR, "LocalFileShareAlbumTags") {
        if (!available()) {
            throw std::runtime_error("ONNX model is unavailable");
        }

        session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session_options_.SetLogSeverityLevel(ORT_LOGGING_LEVEL_ERROR);
        Ort::CUDAProviderOptions cuda_options;
        cuda_options.Update({
            {"device_id", "0"},
            {"cudnn_conv_algo_search", "EXHAUSTIVE"},
            {"cudnn_conv_use_max_workspace", "1"},
            {"do_copy_in_default_stream", "1"},
        });
        session_options_.AppendExecutionProvider_CUDA_V2(*cuda_options);
        session_.reset(new Ort::Session(env_, options_.model_path.c_str(), session_options_));
    }

    bool available() const override {
        return !options_.model_path.empty() && fs::exists(fs::path(options_.model_path));
    }

    std::vector<PhotoTag> predict(const std::wstring& image_path) const override {
        std::vector<float> input = load_image_tensor(image_path);
        std::array<int64_t, 4> input_shape = {1, 3, kInputSize, kInputSize};
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
            OrtAllocatorType::OrtArenaAllocator,
            OrtMemType::OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input.data(),
            input.size(),
            input_shape.data(),
            input_shape.size());

        const char* input_names[] = {"input"};
        const char* output_names[] = {"logits"};
        auto outputs = session_->Run(
            Ort::RunOptions{nullptr},
            input_names,
            &input_tensor,
            1,
            output_names,
            1);

        float* logits = outputs[0].GetTensorMutableData<float>();
        size_t count = metadata_.labels.size();
        std::vector<float> probabilities(count);
        for (size_t i = 0; i < count; ++i) {
            probabilities[i] = 1.0f / (1.0f + std::exp(-logits[i]));
        }

        return derive_tags_from_probabilities(metadata_.labels, metadata_.thresholds, probabilities);
    }

private:
    OnnxPhotoTaggerOptions options_;
    ModelMetadata metadata_;
    Ort::Env env_;
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;
};

} // namespace
#endif

bool NullPhotoTagger::available() const {
    return false;
}

std::vector<PhotoTag> NullPhotoTagger::predict(const std::wstring&) const {
    return {};
}

PythonPhotoTagger::PythonPhotoTagger(PythonPhotoTaggerOptions options)
    : options_(std::move(options)) {}

bool PythonPhotoTagger::available() const {
    if (options_.python_exe.empty() || options_.project_root.empty()) {
        return false;
    }

    return fs::exists(fs::path(options_.python_exe)) &&
           fs::exists(fs::path(options_.project_root) / L"scripts" / L"predict.py");
}

std::vector<PhotoTag> PythonPhotoTagger::predict(const std::wstring& image_path) const {
    if (!available()) {
        return {};
    }

    std::wstring output_path = temp_json_path();
    std::wstring log_path = options_.log_path.empty()
        ? default_log_path(options_.project_root)
        : options_.log_path;
    std::wstring command =
        quote_arg(options_.python_exe) +
        L" scripts\\predict.py" +
        L" --config " + quote_arg(options_.config_path) +
        L" --thresholds " + quote_arg(options_.thresholds_path) +
        L" --image " + quote_arg(image_path) +
        L" --device " + quote_arg(options_.device) +
        L" --output " + quote_arg(output_path);

    SECURITY_ATTRIBUTES security = {};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE log_handle = CreateFileW(
        log_path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        &security,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (log_handle == INVALID_HANDLE_VALUE) {
        DeleteFileW(output_path.c_str());
        throw std::runtime_error("Python photo tagger failed to open log file");
    }

    HANDLE input_handle = CreateFileW(
        L"NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = log_handle;
    startup.hStdError = log_handle;
    startup.hStdInput = input_handle == INVALID_HANDLE_VALUE ? NULL : input_handle;

    PROCESS_INFORMATION process = {};
    std::vector<wchar_t> command_line(command.begin(), command.end());
    command_line.push_back(L'\0');

    BOOL created = CreateProcessW(
        NULL,
        command_line.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        options_.project_root.c_str(),
        &startup,
        &process);

    if (!created) {
        if (input_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(input_handle);
        }
        CloseHandle(log_handle);
        DeleteFileW(output_path.c_str());
        throw std::runtime_error("Python photo tagger failed to start");
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (input_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(input_handle);
    }
    CloseHandle(log_handle);

    if (exit_code != 0) {
        DeleteFileW(output_path.c_str());
        throw std::runtime_error("Python photo tagger failed, see " + wide_to_utf8(log_path));
    }

    std::string json = read_utf8_file(output_path);
    DeleteFileW(output_path.c_str());
    return parse_prediction_json(json);
}

std::unique_ptr<PhotoTagger> create_photo_tagger(
    const OnnxPhotoTaggerOptions& onnx_options,
    const PythonPhotoTaggerOptions& python_options) {
#ifdef LFS_ENABLE_ONNXRUNTIME
    try {
        std::unique_ptr<PhotoTagger> tagger(new OnnxPhotoTagger(onnx_options));
        if (tagger->available()) {
            std::cerr << "Album CV tagger backend: ONNX Runtime CUDA\n";
            return tagger;
        }
    } catch (const std::exception& ex) {
        std::cerr << "ONNX Runtime CUDA photo tagger unavailable: " << ex.what() << "\n";
    }
#else
    (void)onnx_options;
#endif

    std::unique_ptr<PhotoTagger> tagger(new PythonPhotoTagger(python_options));
    if (tagger->available()) {
        std::cerr << "Album CV tagger backend: Python subprocess\n";
        return tagger;
    }

    return std::unique_ptr<PhotoTagger>(new NullPhotoTagger());
}
