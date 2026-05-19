#include "PhotoService.h"

#include "FileManager.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <thread>

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

ScanStatus PhotoService::beginScan() {
    bool expected = false;
    if (!scanning_.compare_exchange_strong(expected, true)) {
        return latestStatus();
    }

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

ScanStatus PhotoService::scanNow() {
    ScanStatus current = beginScan();
    if (current.status != "scanning") {
        return current;
    }

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
            repository_.upsertPhoto(photo);
            ++current.total_seen;
            ++current.total_indexed;
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

    finishScan(current);
    return current;
}

ScanStatus PhotoService::startScanAsync() {
    ScanStatus current = beginScan();
    if (current.status != "scanning") {
        return current;
    }

    std::thread([this, current]() mutable {
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
                repository_.upsertPhoto(photo);
                ++current.total_seen;
                ++current.total_indexed;
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
        finishScan(current);
    }).detach();

    return current;
}

ScanStatus PhotoService::latestStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}
