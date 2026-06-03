#pragma once

#include "PhotoRepository.h"

#include <memory>
#include <string>
#include <vector>

struct PhotoTaggerResult {
    std::vector<PhotoTag> tags;
    std::vector<float> embedding;
    int embedding_dim = 0;
    std::string tag_probs_json;
    std::string model_version;
};

class PhotoTagger {
public:
    virtual ~PhotoTagger() = default;
    virtual bool available() const = 0;
    virtual std::vector<PhotoTag> predict(const std::wstring& image_path) const = 0;
    virtual PhotoTaggerResult analyze(const std::wstring& image_path) const;
};

class NullPhotoTagger : public PhotoTagger {
public:
    bool available() const override;
    std::vector<PhotoTag> predict(const std::wstring& image_path) const override;
};

struct PythonPhotoTaggerOptions {
    std::wstring python_exe;
    std::wstring project_root;
    std::wstring log_path;
    std::wstring config_path = L"configs\\train_v2.yaml";
    std::wstring thresholds_path = L"outputs\\smart_album_v2\\thresholds_v2.json";
    std::wstring device = L"cuda";
};

struct OnnxPhotoTaggerOptions {
    std::wstring model_path;
};

class PythonPhotoTagger : public PhotoTagger {
public:
    explicit PythonPhotoTagger(PythonPhotoTaggerOptions options);

    bool available() const override;
    std::vector<PhotoTag> predict(const std::wstring& image_path) const override;

private:
    PythonPhotoTaggerOptions options_;
};

std::unique_ptr<PhotoTagger> create_photo_tagger(
    const OnnxPhotoTaggerOptions& onnx_options,
    const PythonPhotoTaggerOptions& python_options);
