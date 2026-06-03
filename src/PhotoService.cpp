#include "PhotoService.h"

#include "FileManager.h"
#include "PerceptualHash.h"
#include "PhotoQualityScorer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

int64_t unix_time_now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t file_time_to_unix(fs::file_time_type value) {
    auto system_time = std::chrono::time_point_cast<std::chrono::seconds>(
        value - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return system_time.time_since_epoch().count();
}

std::string normalize_relative_path(const std::wstring& root, const std::wstring& file_path) {
    return relative_url_path(root, file_path);
}

std::string folder_from_relative_path(const std::string& relative_path) {
    size_t slash = relative_path.find_last_of('/');
    if (slash == std::string::npos) {
        return std::string();
    }
    return relative_path.substr(0, slash);
}

std::string hash_root_path(const std::wstring& root) {
    return std::to_string(std::hash<std::wstring>{}(lowercase_path(root)));
}

} // namespace

PhotoService::PhotoService(std::wstring share_root, PhotoRepository& repository)
    : share_root_(std::move(share_root)), repository_(repository) {}

PhotoService::PhotoService(std::wstring share_root, PhotoRepository& repository, PhotoTagger* tagger)
    : share_root_(std::move(share_root)), repository_(repository), tagger_(tagger) {}

ScanStatus PhotoService::beginScan() {
    bool expected = false;
    if (!scanning_.compare_exchange_strong(expected, true)) {
        scan_requested_ = true;
        return latestStatus();
    }

    scan_requested_ = false;
    return beginScanPass();
}

ScanStatus PhotoService::beginScanPass() {
    ScanStatus current;
    current.status = "scanning";
    current.started_at = unix_time_now();
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_ = current;
    }
    return current;
}

void PhotoService::finishScan(const ScanStatus& status) {
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_ = status;
    }
    scanning_ = false;
}

void PhotoService::publishScanProgress(const ScanStatus& status) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_ = status;
}

std::string PhotoService::mediaTypeForExtension(const std::wstring& extension) {
    std::wstring lower = lowercase_path(extension);
    if (lower == L".jpg" || lower == L".jpeg" || lower == L".png" ||
        lower == L".webp" || lower == L".heic") {
        return "image";
    }
    if (lower == L".mp4" || lower == L".webm" || lower == L".mov" ||
        lower == L".m4v" || lower == L".avi") {
        return "video";
    }
    if (lower == L".nef" || lower == L".nrw" || lower == L".cr2" ||
        lower == L".arw" || lower == L".dng" || lower == L".rw2") {
        return "raw";
    }
    return "other";
}

std::string PhotoService::thumbnailStatusForMediaType(const std::string& media_type) {
    return media_type == "image" ? "ready" : "unsupported";
}

PhotoRecord PhotoService::buildRecord(const std::wstring& file_path, int64_t indexed_at) const {
    PhotoRecord photo;
    photo.relative_path = normalize_relative_path(share_root_, file_path);
    fs::path path(file_path);
    photo.absolute_path_hash = hash_root_path(share_root_);
    photo.file_name = wide_to_utf8(path.filename().wstring());
    photo.folder_path = folder_from_relative_path(photo.relative_path);
    photo.extension = wide_to_utf8(lowercase_path(path.extension().wstring()));
    photo.media_type = mediaTypeForExtension(path.extension().wstring());
    photo.size_bytes = static_cast<int64_t>(fs::file_size(path));
    photo.modified_at = file_time_to_unix(fs::last_write_time(path));
    photo.thumbnail_status = thumbnailStatusForMediaType(photo.media_type);
    photo.indexed_at = indexed_at;
    photo.missing = false;
    return photo;
}

bool PhotoService::shouldTagPhoto(const PhotoRecord& photo) const {
    if (tagger_ == nullptr || !tagger_->available() || photo.media_type != "image") {
        return false;
    }

    try {
        PhotoRecord existing = repository_.getPhotoByRelativePath(photo.relative_path);
        if (existing.size_bytes == photo.size_bytes && existing.modified_at == photo.modified_at) {
            return repository_.listPhotoTags(existing.id).empty();
        }
    } catch (...) {
        return true;
    }

    return true;
}

bool should_extract_feature(PhotoRepository& repository, const PhotoRecord& photo) {
    if (photo.media_type != "image") {
        return false;
    }
    try {
        PhotoRecord stored = repository.getPhotoByRelativePath(photo.relative_path);
        std::optional<PhotoFeature> feature = repository.getPhotoFeature(stored.id);
        return !feature.has_value() || feature->image_mtime != photo.modified_at;
    } catch (...) {
        return true;
    }
}

std::string hash_reasons_json(const std::vector<std::string>& reasons) {
    std::string json = "[";
    for (size_t i = 0; i < reasons.size(); ++i) {
        if (i != 0) json += ",";
        json += "\"" + reasons[i] + "\"";
    }
    json += "]";
    return json;
}

void PhotoService::tagPhotoIfAvailable(const PhotoRecord& photo, const std::wstring& file_path, bool force) const {
    if (!force || tagger_ == nullptr || !tagger_->available() || photo.media_type != "image") {
        return;
    }

    try {
        PhotoTaggerResult analysis = tagger_->analyze(file_path);
        std::vector<PhotoTag> tags = analysis.tags;
        repository_.replacePhotoTags(photo.relative_path, tags);
    } catch (...) {
    }
}

bool extractFeatureIfNeeded(
    PhotoRepository& repository,
    PhotoTagger* tagger,
    const PhotoRecord& photo,
    const std::wstring& file_path,
    bool force) {
    if (!force || photo.media_type != "image") {
        return false;
    }

    try {
        PhotoRecord stored = repository.getPhotoByRelativePath(photo.relative_path);
        PhotoQualityScore quality = PhotoQualityScorer::score(file_path);
        PhotoFeature feature;
        feature.photo_id = stored.id;
        feature.dhash = PerceptualHash::toHex(PerceptualHash::dhash(file_path));
        feature.phash = feature.dhash;
        feature.sharpness_score = quality.sharpness_score;
        feature.exposure_score = quality.exposure_score;
        feature.resolution_score = quality.resolution_score;
        feature.contrast_score = quality.contrast_score;
        feature.noise_score = quality.noise_score;
        feature.quality_score = quality.quality_score;
        feature.image_mtime = photo.modified_at;
        feature.created_at = unix_time_now();
        feature.updated_at = feature.created_at;

        if (tagger != nullptr && tagger->available()) {
            PhotoTaggerResult analysis = tagger->analyze(file_path);
            feature.embedding = std::move(analysis.embedding);
            feature.embedding_dim = analysis.embedding_dim;
            feature.tag_probs_json = analysis.tag_probs_json;
            feature.model_version = analysis.model_version;
            repository.replacePhotoTags(photo.relative_path, analysis.tags);
        }

        repository.upsertPhotoFeature(feature);
        return true;
    } catch (...) {
        return false;
    }
}

ScanStatus PhotoService::scanOnce(ScanStatus current) {
    try {
        std::set<std::string> seen;
        const int64_t indexed_at = unix_time_now();

        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(share_root_)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string media_type = mediaTypeForExtension(entry.path().extension().wstring());
            if (media_type == "other") {
                continue;
            }

            PhotoRecord photo = buildRecord(entry.path().wstring(), indexed_at);
            seen.insert(photo.relative_path);
            bool needs_tagging = shouldTagPhoto(photo);
            bool needs_feature = should_extract_feature(repository_, photo);
            repository_.upsertPhoto(photo);
            bool feature_extracted = extractFeatureIfNeeded(repository_, tagger_, photo, entry.path().wstring(), needs_feature);
            if (!feature_extracted) {
                tagPhotoIfAvailable(photo, entry.path().wstring(), needs_tagging);
            }
            ++current.total_seen;
            ++current.total_indexed;
            if (current.total_seen % 5 == 0) {
                publishScanProgress(current);
            }
        }

        for (const std::string& indexed_path : repository_.listIndexedRelativePaths()) {
            if (seen.find(indexed_path) == seen.end()) {
                repository_.markMissing(indexed_path);
                ++current.total_removed;
            }
        }

        current.status = "completed";
        current.finished_at = unix_time_now();
    } catch (const std::exception& ex) {
        current.status = "failed";
        current.finished_at = unix_time_now();
        current.error_message = ex.what();
    }
    return current;
}

ScanStatus PhotoService::scanNow() {
    ScanStatus current = beginScan();
    if (current.status != "scanning") {
        return current;
    }

    current = scanOnce(current);

    finishScan(current);
    return current;
}

void PhotoService::runAsyncScans(ScanStatus current) {
    while (true) {
        current = scanOnce(current);
        if (current.status == "failed") {
            finishScan(current);
            return;
        }

        if (scan_requested_.exchange(false)) {
            current = beginScanPass();
            continue;
        }

        finishScan(current);
        if (!scan_requested_.exchange(false)) {
            return;
        }

        bool expected = false;
        if (!scanning_.compare_exchange_strong(expected, true)) {
            return;
        }
        current = beginScanPass();
    }
}

ScanStatus PhotoService::startScanAsync() {
    ScanStatus current = beginScan();
    if (current.status != "scanning") {
        return current;
    }

    std::thread([this, current]() mutable {
        runAsyncScans(current);
    }).detach();

    return current;
}

ScanStatus PhotoService::latestStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}
