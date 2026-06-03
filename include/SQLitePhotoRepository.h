#pragma once

#include "PhotoRepository.h"

#include <mutex>
#include <string>

struct sqlite3;

class SQLitePhotoRepository : public PhotoRepository {
public:
    explicit SQLitePhotoRepository(std::wstring db_path);
    ~SQLitePhotoRepository() override;

    SQLitePhotoRepository(const SQLitePhotoRepository&) = delete;
    SQLitePhotoRepository& operator=(const SQLitePhotoRepository&) = delete;

    void initialize() override;
    int schemaVersion() const override;
    bool hasTable(const std::string& table_name) const;

    void upsertPhoto(const PhotoRecord& photo) override;
    PhotoRecord getPhoto(int64_t id) const override;
    PhotoRecord getPhotoByRelativePath(const std::string& relative_path) const override;
    std::vector<PhotoRecord> listTimeline(const TimelineQuery& query) const override;
    std::vector<PhotoRecord> searchPhotos(const PhotoSearchQuery& query) const override;
    std::vector<FolderRecord> listFolders() const override;
    std::vector<std::string> listIndexedRelativePaths() const override;
    void replacePhotoTags(const std::string& relative_path, const std::vector<PhotoTag>& tags) override;
    std::vector<PhotoTag> listPhotoTags(int64_t photo_id) const override;
    void upsertPhotoFeature(const PhotoFeature& feature) override;
    std::optional<PhotoFeature> getPhotoFeature(int64_t photo_id) const override;
    std::vector<PhotoFeatureRecord> listPhotoFeatures() const override;
    void replaceTidyResults(
        const std::vector<SimilarGroupRecord>& groups,
        const std::vector<DeleteCandidateRecord>& candidates) override;
    std::vector<SimilarGroupRecord> listSimilarGroups() const override;
    std::vector<DeleteCandidateRecord> listDeleteCandidates() const override;
    void updateDeleteCandidateStatus(const std::string& candidate_id, const std::string& status) override;
    void markMissing(const std::string& relative_path) override;
    void toggleFavorite(int64_t id, bool favorite) override;
    void deletePhoto(int64_t id) override;

private:
    void open() const;
    void exec(const char* sql) const;
    void refreshFolderStats(const std::string& folder_path) const;

    std::wstring db_path_;
    mutable sqlite3* db_ = nullptr;
    mutable std::recursive_mutex mutex_;
};
