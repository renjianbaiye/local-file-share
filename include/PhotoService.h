#pragma once

#include "PhotoRepository.h"
#include "PhotoTagger.h"

#include <atomic>
#include <mutex>
#include <string>

struct ScanStatus {
    std::string status = "idle";
    int64_t started_at = 0;
    int64_t finished_at = 0;
    int64_t total_seen = 0;
    int64_t total_indexed = 0;
    int64_t total_updated = 0;
    int64_t total_removed = 0;
    std::string error_message;
};

class PhotoService {
public:
    PhotoService(std::wstring share_root, PhotoRepository& repository);
    PhotoService(std::wstring share_root, PhotoRepository& repository, PhotoTagger* tagger);

    ScanStatus scanNow();
    ScanStatus startScanAsync();
    ScanStatus latestStatus() const;

    static std::string mediaTypeForExtension(const std::wstring& extension);
    static std::string thumbnailStatusForMediaType(const std::string& media_type);

private:
    ScanStatus beginScan();
    ScanStatus beginScanPass();
    ScanStatus scanOnce(ScanStatus current);
    void runAsyncScans(ScanStatus current);
    void finishScan(const ScanStatus& status);
    void publishScanProgress(const ScanStatus& status);
    PhotoRecord buildRecord(const std::wstring& file_path, int64_t indexed_at) const;
    bool shouldTagPhoto(const PhotoRecord& photo) const;
    void tagPhotoIfAvailable(const PhotoRecord& photo, const std::wstring& file_path, bool force) const;

    std::wstring share_root_;
    PhotoRepository& repository_;
    PhotoTagger* tagger_ = nullptr;
    mutable std::mutex status_mutex_;
    ScanStatus status_;
    std::atomic<bool> scanning_{false};
    std::atomic<bool> scan_requested_{false};
};
