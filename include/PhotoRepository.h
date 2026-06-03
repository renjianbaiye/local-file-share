#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct PhotoTag {
    std::string tag;
    double probability = 0.0;
    double threshold = 0.0;
    bool predicted = false;
    bool derived = false;
};

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

struct PhotoFeature {
    int64_t photo_id = 0;
    std::vector<float> embedding;
    int embedding_dim = 0;
    std::string phash;
    std::string dhash;
    double sharpness_score = 0.0;
    double exposure_score = 0.0;
    double resolution_score = 0.0;
    double contrast_score = 0.0;
    double noise_score = 0.0;
    double quality_score = 0.0;
    std::string tag_probs_json;
    std::string model_version;
    int64_t image_mtime = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

struct PhotoSearchQuery {
    std::string keyword;
    int limit = 80;
    std::string folder_path;
    std::vector<std::string> media_types;
    std::optional<bool> favorite;
    std::optional<bool> missing = false;
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

struct PhotoFeatureRecord {
    PhotoRecord photo;
    PhotoFeature feature;
};

struct SimilarGroupPhotoRecord {
    std::string group_id;
    int64_t photo_id = 0;
    double similarity_to_best = 0.0;
    int hash_distance_to_best = -1;
    double quality_score = 0.0;
    std::string recommendation;
    std::string reasons_json;
};

struct SimilarGroupRecord {
    std::string group_id;
    std::string group_type;
    std::string scene_id;
    int64_t best_photo_id = 0;
    int64_t cover_photo_id = 0;
    double confidence = 0.0;
    std::string reason;
    int keep_count = 0;
    int review_count = 0;
    int delete_candidate_count = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
    std::vector<SimilarGroupPhotoRecord> photos;
};

struct DeleteCandidateRecord {
    std::string candidate_id;
    int64_t photo_id = 0;
    std::string group_id;
    int64_t matched_best_photo_id = 0;
    double similarity_to_best = 0.0;
    double quality_score = 0.0;
    double best_quality_score = 0.0;
    double safe_to_delete_score = 0.0;
    std::string reason;
    bool requires_user_confirmation = true;
    std::string status = "pending";
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

struct TidyReport {
    int total_photos = 0;
    int total_similar_groups = 0;
    int total_delete_candidates = 0;
    int total_review = 0;
    int total_keep = 0;
    std::string groups_by_type_json = "{}";
    int missing_embeddings = 0;
    int skipped_photos = 0;
    double average_group_size = 0.0;
    int estimated_reclaimable_count = 0;
    bool conservative_mode = true;
    bool delete_decision_uses_labels = false;
    bool tag_score_used = false;
    bool embedding_based_grouping = true;
    bool hash_based_duplicate_detection = true;
    bool quality_based_recommendation = true;
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
    virtual std::vector<PhotoRecord> searchPhotos(const PhotoSearchQuery& query) const = 0;
    virtual std::vector<FolderRecord> listFolders() const = 0;
    virtual std::vector<std::string> listIndexedRelativePaths() const = 0;
    virtual void replacePhotoTags(const std::string& relative_path, const std::vector<PhotoTag>& tags) = 0;
    virtual std::vector<PhotoTag> listPhotoTags(int64_t photo_id) const = 0;
    virtual void upsertPhotoFeature(const PhotoFeature& feature) = 0;
    virtual std::optional<PhotoFeature> getPhotoFeature(int64_t photo_id) const = 0;
    virtual std::vector<PhotoFeatureRecord> listPhotoFeatures() const = 0;
    virtual void replaceTidyResults(
        const std::vector<SimilarGroupRecord>& groups,
        const std::vector<DeleteCandidateRecord>& candidates) = 0;
    virtual std::vector<SimilarGroupRecord> listSimilarGroups() const = 0;
    virtual std::vector<DeleteCandidateRecord> listDeleteCandidates() const = 0;
    virtual void updateDeleteCandidateStatus(const std::string& candidate_id, const std::string& status) = 0;
    virtual void markMissing(const std::string& relative_path) = 0;
    virtual void toggleFavorite(int64_t id, bool favorite) = 0;
    virtual void deletePhoto(int64_t id) = 0;
};
