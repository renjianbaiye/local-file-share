#include "PerceptualHash.h"
#include "PhotoImage.h"
#include "PhotoQualityScorer.h"
#include "SQLitePhotoRepository.h"
#include "TidyEngine.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

static std::wstring temp_db_path() {
    fs::path path = fs::temp_directory_path() / L"local-file-share-tidy-engine-test.db";
    fs::remove(path);
    fs::remove(path.wstring() + L"-wal");
    fs::remove(path.wstring() + L"-shm");
    return path.wstring();
}

static void write_bmp(const fs::path& path, int width, int height, bool checker) {
    fs::create_directories(path.parent_path());
    int row_stride = ((width * 3 + 3) / 4) * 4;
    int pixel_bytes = row_stride * height;
    int file_size = 54 + pixel_bytes;
    std::vector<unsigned char> data(file_size, 0);
    data[0] = 'B';
    data[1] = 'M';
    *reinterpret_cast<int32_t*>(&data[2]) = file_size;
    *reinterpret_cast<int32_t*>(&data[10]) = 54;
    *reinterpret_cast<int32_t*>(&data[14]) = 40;
    *reinterpret_cast<int32_t*>(&data[18]) = width;
    *reinterpret_cast<int32_t*>(&data[22]) = height;
    *reinterpret_cast<int16_t*>(&data[26]) = 1;
    *reinterpret_cast<int16_t*>(&data[28]) = 24;

    for (int y = 0; y < height; ++y) {
        unsigned char* row = &data[54 + y * row_stride];
        for (int x = 0; x < width; ++x) {
            unsigned char value = checker && ((x / 4 + y / 4) % 2 == 0) ? 235 : 80;
            row[x * 3 + 0] = value;
            row[x * 3 + 1] = value;
            row[x * 3 + 2] = value;
        }
    }

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

static PhotoRecord make_photo(const std::string& path, int64_t captured_at, bool favorite = false) {
    PhotoRecord photo;
    photo.relative_path = path;
    photo.absolute_path_hash = "root";
    photo.file_name = path.substr(path.find_last_of('/') + 1);
    photo.folder_path = "camera";
    photo.extension = ".jpg";
    photo.media_type = "image";
    photo.mime_type = "image/jpeg";
    photo.size_bytes = 1000;
    photo.modified_at = captured_at;
    photo.captured_at = captured_at;
    photo.thumbnail_status = "ready";
    photo.indexed_at = captured_at;
    photo.is_favorite = favorite;
    return photo;
}

static std::vector<float> unit_embedding(float x, float y) {
    std::vector<float> values(1024, 0.0f);
    values[0] = x;
    values[1] = y;
    float norm = std::sqrt(x * x + y * y);
    values[0] /= norm;
    values[1] /= norm;
    return values;
}

int main() {
    try {
        fs::path root = fs::temp_directory_path() / L"local-file-share-tidy-quality-test";
        fs::remove_all(root);
        write_bmp(root / L"checker.bmp", 64, 64, true);
        write_bmp(root / L"flat.bmp", 64, 64, false);
        write_bmp(root / L"large.bmp", 2200, 1900, true);

        PhotoQualityScore quality = PhotoQualityScorer::score(root / L"checker.bmp");
        assert(quality.quality_score >= 0.0 && quality.quality_score <= 1.0);
        assert(quality.sharpness_score >= 0.0 && quality.sharpness_score <= 1.0);
        assert(quality.exposure_score >= 0.0 && quality.exposure_score <= 1.0);

        RgbImage large_preview = load_rgb_image_max(root / L"large.bmp", 1024);
        assert(large_preview.original_width == 2200);
        assert(large_preview.original_height == 1900);
        assert(large_preview.width <= 1024);
        assert(large_preview.height <= 1024);

        PhotoQualityScore large_quality = PhotoQualityScorer::score(root / L"large.bmp");
        assert(std::abs(large_quality.resolution_score - 0.8) < 0.0001);

        uint64_t first_hash = PerceptualHash::dhash(root / L"checker.bmp");
        uint64_t same_hash = PerceptualHash::dhash(root / L"checker.bmp");
        uint64_t flat_hash = PerceptualHash::dhash(root / L"flat.bmp");
        assert(first_hash == same_hash);
        assert(PerceptualHash::hammingDistance(first_hash, same_hash) == 0);
        assert(PerceptualHash::hammingDistance(first_hash, flat_hash) >= 0);

        std::wstring db_path = temp_db_path();
        {
            SQLitePhotoRepository repository(db_path);
            repository.initialize();
            assert(repository.schemaVersion() >= 3);
            assert(repository.hasTable("photo_features"));
            assert(repository.hasTable("similar_groups"));
            assert(repository.hasTable("similar_group_photos"));
            assert(repository.hasTable("delete_candidates"));

            repository.upsertPhoto(make_photo("camera/best.jpg", 1779166800));
            repository.upsertPhoto(make_photo("camera/weak.jpg", 1779166860));
            repository.upsertPhoto(make_photo("camera/review.jpg", 1779166920));

            PhotoRecord best = repository.getPhotoByRelativePath("camera/best.jpg");
            PhotoRecord weak = repository.getPhotoByRelativePath("camera/weak.jpg");
            PhotoRecord review = repository.getPhotoByRelativePath("camera/review.jpg");

            PhotoFeature best_feature;
            best_feature.photo_id = best.id;
            best_feature.embedding = unit_embedding(1.0f, 0.0f);
            best_feature.embedding_dim = 1024;
            best_feature.dhash = "0000000000000000";
            best_feature.phash = "0000000000000000";
            best_feature.quality_score = 0.95;
            best_feature.tag_probs_json = "{\"bad\":0.99}";
            best_feature.model_version = "test";
            best_feature.image_mtime = best.modified_at;
            repository.upsertPhotoFeature(best_feature);

            PhotoFeature weak_feature = best_feature;
            weak_feature.photo_id = weak.id;
            weak_feature.embedding = unit_embedding(0.98f, 0.2f);
            weak_feature.dhash = "000000000000000f";
            weak_feature.phash = "000000000000000f";
            weak_feature.quality_score = 0.70;
            weak_feature.image_mtime = weak.modified_at;
            repository.upsertPhotoFeature(weak_feature);

            PhotoFeature review_feature = best_feature;
            review_feature.photo_id = review.id;
            review_feature.embedding = unit_embedding(0.96f, 0.28f);
            review_feature.dhash = "ffffffffffffffff";
            review_feature.phash = "ffffffffffffffff";
            review_feature.quality_score = 0.90;
            review_feature.image_mtime = review.modified_at;
            repository.upsertPhotoFeature(review_feature);

            TidyReport report = TidyEngine(repository).rebuild();
            assert(report.total_similar_groups == 1);
            assert(report.total_delete_candidates == 1);
            assert(report.delete_decision_uses_labels == false);
            assert(report.tag_score_used == false);

            std::vector<DeleteCandidateRecord> candidates = repository.listDeleteCandidates();
            assert(candidates.size() == 1);
            assert(candidates[0].photo_id == weak.id);
            assert(candidates[0].requires_user_confirmation == true);
            assert(candidates[0].status == "pending");

            repository.updateDeleteCandidateStatus(candidates[0].candidate_id, "confirmed");
            candidates = repository.listDeleteCandidates();
            assert(candidates[0].status == "confirmed");
            assert(fs::exists(root / L"checker.bmp"));
        }

        fs::remove_all(root);
        fs::remove(db_path);
        fs::remove(db_path + L"-wal");
        fs::remove(db_path + L"-shm");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "TidyEngineTests failed: " << ex.what() << "\n";
        return 1;
    }
}
