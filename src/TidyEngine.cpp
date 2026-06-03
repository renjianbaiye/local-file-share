#include "TidyEngine.h"

#include "PerceptualHash.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {

int64_t now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

double cosine(const std::vector<float>& left, const std::vector<float>& right) {
    if (left.empty() || right.empty() || left.size() != right.size()) {
        return -1.0;
    }
    double dot = 0.0;
    double left_norm = 0.0;
    double right_norm = 0.0;
    for (size_t i = 0; i < left.size(); ++i) {
        dot += static_cast<double>(left[i]) * right[i];
        left_norm += static_cast<double>(left[i]) * left[i];
        right_norm += static_cast<double>(right[i]) * right[i];
    }
    if (left_norm <= 0.0 || right_norm <= 0.0) {
        return -1.0;
    }
    return dot / (std::sqrt(left_norm) * std::sqrt(right_norm));
}

int hash_distance(const PhotoFeature& left, const PhotoFeature& right) {
    int dhash = PerceptualHash::hammingDistanceHex(left.dhash, right.dhash);
    int phash = PerceptualHash::hammingDistanceHex(left.phash, right.phash);
    if (dhash < 0) return phash;
    if (phash < 0) return dhash;
    return std::min(dhash, phash);
}

int64_t sort_time(const PhotoRecord& photo) {
    return photo.captured_at.value_or(photo.modified_at);
}

double minutes_between(const PhotoRecord& left, const PhotoRecord& right) {
    return std::abs(static_cast<double>(sort_time(left) - sort_time(right))) / 60.0;
}

std::string json_array(const std::vector<std::string>& values) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out << ",";
        out << "\"" << values[i] << "\"";
    }
    out << "]";
    return out.str();
}

std::string hash_prefix(const std::string& value) {
    return value.size() <= 4 ? value : value.substr(0, 4);
}

using Pair = std::pair<int, int>;

Pair make_pair_key(int left, int right) {
    return left < right ? Pair{left, right} : Pair{right, left};
}

std::set<Pair> build_candidate_pairs(const std::vector<PhotoFeatureRecord>& records) {
    std::map<int64_t, std::vector<int>> hour_buckets;
    std::map<int64_t, std::vector<int>> day_buckets;
    std::map<std::string, std::vector<int>> hash_buckets;
    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        int64_t time_value = sort_time(records[i].photo);
        hour_buckets[time_value / 3600].push_back(i);
        day_buckets[time_value / 86400].push_back(i);
        if (!records[i].feature.dhash.empty()) {
            hash_buckets[hash_prefix(records[i].feature.dhash)].push_back(i);
        }
        if (!records[i].feature.phash.empty()) {
            hash_buckets[hash_prefix(records[i].feature.phash)].push_back(i);
        }
    }

    std::set<Pair> pairs;
    auto add_bucket_pairs = [&](const std::vector<int>& bucket) {
        for (size_t i = 0; i < bucket.size(); ++i) {
            for (size_t j = i + 1; j < bucket.size(); ++j) {
                pairs.insert(make_pair_key(bucket[i], bucket[j]));
            }
        }
    };
    for (const auto& item : hour_buckets) add_bucket_pairs(item.second);
    for (const auto& item : hash_buckets) add_bucket_pairs(item.second);
    for (const auto& item : day_buckets) {
        if (item.second.size() <= 256) {
            add_bucket_pairs(item.second);
        }
    }
    return pairs;
}

bool should_connect(const PhotoFeatureRecord& left, const PhotoFeatureRecord& right) {
    int hdist = hash_distance(left.feature, right.feature);
    if (hdist >= 0 && hdist <= 6) {
        return true;
    }
    double sim = cosine(left.feature.embedding, right.feature.embedding);
    bool has_time = left.photo.captured_at.has_value() && right.photo.captured_at.has_value();
    if (!has_time) {
        return sim >= 0.94;
    }
    double diff = minutes_between(left.photo, right.photo);
    return (sim >= 0.92 && diff <= 10.0) || (sim >= 0.94 && diff <= 10.0);
}

std::vector<std::vector<int>> components(
    int count,
    const std::set<Pair>& connected_pairs) {
    std::vector<std::vector<int>> adjacency(static_cast<size_t>(count));
    for (const auto& pair : connected_pairs) {
        adjacency[pair.first].push_back(pair.second);
        adjacency[pair.second].push_back(pair.first);
    }
    std::vector<int> seen(static_cast<size_t>(count), 0);
    std::vector<std::vector<int>> result;
    for (int i = 0; i < count; ++i) {
        if (seen[i] || adjacency[i].empty()) continue;
        std::queue<int> queue;
        queue.push(i);
        seen[i] = 1;
        std::vector<int> component;
        while (!queue.empty()) {
            int current = queue.front();
            queue.pop();
            component.push_back(current);
            for (int next : adjacency[current]) {
                if (!seen[next]) {
                    seen[next] = 1;
                    queue.push(next);
                }
            }
        }
        if (component.size() >= 2) {
            result.push_back(component);
        }
    }
    return result;
}

} // namespace

TidyEngine::TidyEngine(PhotoRepository& repository)
    : repository_(repository) {}

TidyReport TidyEngine::rebuild() {
    std::vector<PhotoFeatureRecord> records = repository_.listPhotoFeatures();
    std::set<Pair> connected_pairs;
    std::set<Pair> candidate_pairs = build_candidate_pairs(records);
    for (const Pair& pair : candidate_pairs) {
        if (should_connect(records[pair.first], records[pair.second])) {
            connected_pairs.insert(pair);
        }
    }

    std::vector<std::vector<int>> comps = components(static_cast<int>(records.size()), connected_pairs);
    std::vector<SimilarGroupRecord> groups;
    std::vector<DeleteCandidateRecord> candidates;
    std::map<std::string, int> groups_by_type;
    int keep_count = 0;
    int review_count = 0;
    int64_t now = now_seconds();

    for (size_t group_index = 0; group_index < comps.size(); ++group_index) {
        const std::vector<int>& comp = comps[group_index];
        int best_index = *std::max_element(comp.begin(), comp.end(), [&](int left, int right) {
            return records[left].feature.quality_score < records[right].feature.quality_score;
        });
        const PhotoFeatureRecord& best = records[best_index];

        bool duplicate_group = false;
        for (size_t i = 0; i < comp.size(); ++i) {
            for (size_t j = i + 1; j < comp.size(); ++j) {
                int hdist = hash_distance(records[comp[i]].feature, records[comp[j]].feature);
                if (hdist >= 0 && hdist <= 6) duplicate_group = true;
            }
        }

        SimilarGroupRecord group;
        std::ostringstream group_id;
        group_id << "similar_" << (group_index + 1);
        group.group_id = group_id.str();
        group.group_type = duplicate_group ? "duplicate" : "burst";
        group.best_photo_id = best.photo.id;
        group.cover_photo_id = best.photo.id;
        group.confidence = duplicate_group ? 0.98 : 0.92;
        group.reason = duplicate_group
            ? "dHash/pHash strong duplicate signals; recommendations use quality score."
            : "High DINOv2 embedding similarity in a short time window.";
        group.created_at = now;
        group.updated_at = now;
        groups_by_type[group.group_type]++;

        for (int record_index : comp) {
            const PhotoFeatureRecord& current = records[record_index];
            double sim_to_best = current.photo.id == best.photo.id
                ? 1.0
                : cosine(current.feature.embedding, best.feature.embedding);
            int hdist_to_best = current.photo.id == best.photo.id
                ? 0
                : hash_distance(current.feature, best.feature);
            double quality_gap = best.feature.quality_score - current.feature.quality_score;
            bool strong_duplicate = hdist_to_best >= 0 && hdist_to_best <= 6;

            SimilarGroupPhotoRecord photo;
            photo.group_id = group.group_id;
            photo.photo_id = current.photo.id;
            photo.similarity_to_best = sim_to_best;
            photo.hash_distance_to_best = hdist_to_best;
            photo.quality_score = current.feature.quality_score;

            std::vector<std::string> reasons;
            if (current.photo.id == best.photo.id) {
                photo.recommendation = "keep";
                reasons.push_back("best_quality_in_group");
                ++keep_count;
                ++group.keep_count;
            } else if (current.photo.is_favorite) {
                photo.recommendation = "review";
                reasons.push_back("favorite_or_protected");
                ++review_count;
                ++group.review_count;
            } else if (sim_to_best < 0.90) {
                photo.recommendation = "review";
                reasons.push_back("similarity_to_best<0.90");
                ++review_count;
                ++group.review_count;
            } else if (quality_gap < 0.08) {
                photo.recommendation = "review";
                reasons.push_back("quality_gap<0.08");
                ++review_count;
                ++group.review_count;
            } else if (
                (strong_duplicate && comp.size() >= 2)
                || (sim_to_best >= 0.92 && quality_gap >= 0.12 && comp.size() >= 3)) {
                photo.recommendation = "delete_candidate";
                reasons.push_back(strong_duplicate ? "strong_hash_duplicate_lower_quality" : "high_similarity_lower_quality");
                ++group.delete_candidate_count;

                DeleteCandidateRecord candidate;
                candidate.candidate_id = group.group_id + "_" + std::to_string(current.photo.id);
                candidate.photo_id = current.photo.id;
                candidate.group_id = group.group_id;
                candidate.matched_best_photo_id = best.photo.id;
                candidate.similarity_to_best = sim_to_best;
                candidate.quality_score = current.feature.quality_score;
                candidate.best_quality_score = best.feature.quality_score;
                candidate.safe_to_delete_score = std::max(0.0, std::min(1.0, quality_gap + sim_to_best - 0.9));
                candidate.reason = reasons[0];
                candidate.requires_user_confirmation = true;
                candidate.status = "pending";
                candidate.created_at = now;
                candidate.updated_at = now;
                candidates.push_back(candidate);
            } else {
                photo.recommendation = "review";
                reasons.push_back("conservative_review");
                ++review_count;
                ++group.review_count;
            }
            photo.reasons_json = json_array(reasons);
            group.photos.push_back(photo);
        }
        groups.push_back(group);
    }

    repository_.replaceTidyResults(groups, candidates);

    int grouped_photos = 0;
    for (const auto& group : groups) grouped_photos += static_cast<int>(group.photos.size());
    std::ostringstream group_json;
    group_json << "{";
    bool first = true;
    for (const auto& item : groups_by_type) {
        if (!first) group_json << ",";
        first = false;
        group_json << "\"" << item.first << "\":" << item.second;
    }
    group_json << "}";

    TidyReport report;
    report.total_photos = static_cast<int>(records.size());
    report.total_similar_groups = static_cast<int>(groups.size());
    report.total_delete_candidates = static_cast<int>(candidates.size());
    report.total_review = review_count;
    report.total_keep = keep_count;
    report.groups_by_type_json = group_json.str();
    report.missing_embeddings = static_cast<int>(std::count_if(records.begin(), records.end(), [](const PhotoFeatureRecord& record) {
        return record.feature.embedding.empty();
    }));
    report.skipped_photos = static_cast<int>(records.size()) - grouped_photos;
    report.average_group_size = groups.empty() ? 0.0 : static_cast<double>(grouped_photos) / groups.size();
    report.estimated_reclaimable_count = static_cast<int>(candidates.size());
    return report;
}
