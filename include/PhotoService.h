#pragma once

#include "PhotoRepository.h"

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

    ScanStatus scanNow();
    ScanStatus startScanAsync();
    ScanStatus latestStatus() const;

    static std::string mediaTypeForExtension(const std::wstring& extension);
    static std::string thumbnailStatusForMediaType(const std::string& media_type);

private:
    ScanStatus beginScan();
    void finishScan(const ScanStatus& status);
    PhotoRecord buildRecord(const std::wstring& file_path, int64_t indexed_at) const;

    std::wstring share_root_;
    PhotoRepository& repository_;
    mutable std::mutex status_mutex_;
    ScanStatus status_;
    std::atomic<bool> scanning_{false};
};
