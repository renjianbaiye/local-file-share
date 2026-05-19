#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct PhotoRecord {
    int64_t id = 0;
    std::string relative_path;
    std::string absolute_path_hash;
    std::string file_name;
    std::string folder_path;
    std::string extension;
    std::string media_type;
    std::string mime_type;
    int64_t size_bytes = 0;
    int64_t modified_at = 0;
    std::optional<int64_t> captured_at;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> orientation;
    std::string content_hash;
    std::string thumbnail_status = "pending";
    std::string thumbnail_path;
    bool is_favorite = false;
    int64_t indexed_at = 0;
    bool missing = false;
};

struct TimelineQuery {
    int limit = 100;
    std::string cursor;
    std::string folder_path;
    std::vector<std::string> media_types;
    std::optional<bool> favorite;
    std::optional<bool> missing;
};

struct FolderRecord {
    std::string relative_path;
    int64_t photo_count = 0;
    int64_t video_count = 0;
    int64_t raw_count = 0;
    std::optional<int64_t> latest_captured_at;
    std::optional<int64_t> latest_modified_at;
    int64_t indexed_at = 0;
};

class PhotoRepository {
public:
    virtual ~PhotoRepository() = default;

    virtual void initialize() = 0;
    virtual int schemaVersion() const = 0;
    virtual void upsertPhoto(const PhotoRecord& photo) = 0;
    virtual PhotoRecord getPhoto(int64_t id) const = 0;
    virtual PhotoRecord getPhotoByRelativePath(const std::string& relative_path) const = 0;
    virtual std::vector<PhotoRecord> listTimeline(const TimelineQuery& query) const = 0;
    virtual std::vector<FolderRecord> listFolders() const = 0;
    virtual std::vector<std::string> listIndexedRelativePaths() const = 0;
    virtual void markMissing(const std::string& relative_path) = 0;
    virtual void toggleFavorite(int64_t id, bool favorite) = 0;
};
