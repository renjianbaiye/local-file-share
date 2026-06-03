#include "Server.h"
#include "AppOptions.h"
#include "FileManager.h"
#include "HtmlRenderer.h"
#include "PhotoService.h"
#include "PhotoTagger.h"
#include "SQLitePhotoRepository.h"
#include "TidyEngine.h"
#include "httplib.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <bcrypt.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Token generation
// ---------------------------------------------------------------------------

static std::string generate_token() {
    unsigned char bytes[16];
    NTSTATUS status = BCryptGenRandom(NULL, bytes, sizeof(bytes),
                                     BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
        // Fallback: mix time + process id (less secure but functional)
        srand(static_cast<unsigned>(time(NULL)) ^ GetCurrentProcessId());
        for (int i = 0; i < 16; ++i) {
            bytes[i] = static_cast<unsigned char>(rand() % 256);
        }
    }

    char hex[33];
    for (int i = 0; i < 16; ++i) {
        snprintf(hex + i * 2, 3, "%02x", bytes[i]);
    }
    return std::string(hex, 32);
}

// ---------------------------------------------------------------------------
// Cookie parsing – strict key match, not substring
// ---------------------------------------------------------------------------

static std::string trim_whitespace(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
        --end;
    }
    return s.substr(start, end - start);
}

static bool has_valid_token_cookie(const httplib::Request& req, const std::string& token) {
    std::string cookie = req.get_header_value("Cookie");
    if (cookie.empty()) {
        return false;
    }

    size_t pos = 0;
    while (pos < cookie.size()) {
        size_t semicolon = cookie.find(';', pos);
        std::string part;
        if (semicolon == std::string::npos) {
            part = cookie.substr(pos);
            pos = cookie.size();
        } else {
            part = cookie.substr(pos, semicolon - pos);
            pos = semicolon + 1;
        }

        std::string trimmed = trim_whitespace(part);
        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = trim_whitespace(trimmed.substr(0, eq));
        std::string value = trimmed.substr(eq + 1);

        if (key == "lfs_token" && value == token) {
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// 403 page
// ---------------------------------------------------------------------------

static void send_auth_denied(httplib::Response& res) {
    res.status = 403;
    std::string html =
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>Access Denied</title>"
        "<style>*{box-sizing:border-box}"
        "body{margin:0;display:grid;place-items:center;min-height:100vh;"
        "font-family:\"SF Pro Display\",\"Segoe UI\",Helvetica,Arial,sans-serif;"
        "background:radial-gradient(circle at 78% 10%,rgba(85,159,255,.18),transparent 28%),"
        "linear-gradient(135deg,#fbfcff,#eef3f8);color:#16181d;padding:22px;text-align:center}"
        "main{width:min(480px,100%);background:rgba(255,255,255,.78);"
        "border:1px solid rgba(25,31,44,.11);border-radius:32px;padding:36px;"
        "box-shadow:0 24px 70px rgba(21,30,48,.11);backdrop-filter:blur(24px)}"
        ".code{font-size:72px;font-weight:800;color:#e5e7eb;line-height:1;margin:0 0 8px}"
        "h1{font-size:22px;margin:0 0 12px}"
        "p{color:#6b7280;line-height:1.65;margin:0}"
        "</style></head><body><main>"
        "<div class=\"code\">403</div>"
        "<h1>&#35775;&#38382;&#34987;&#25298;&#32477;</h1>"
        "<p>&#27492;&#20849;&#20139;&#31354;&#38388;&#38656;&#35201;&#26377;&#25928;&#30340;&#35775;&#38382;&#38142;&#25509;&#12290;"
        "&#35831;&#25195;&#25551;&#20027;&#26426;&#23631;&#24149;&#19978;&#30340;&#20108;&#32500;&#30721;&#65292;"
        "&#25110;&#21521;&#20998;&#20139;&#32773;&#32034;&#21462;&#27491;&#30830;&#30340; URL&#12290;</p>"
        "</main></body></html>";
    res.set_content(html, "text/html; charset=utf-8");
}

// ---------------------------------------------------------------------------
// Network helpers
// ---------------------------------------------------------------------------

static bool is_private_ipv4(const std::string& ip) {
    unsigned int a = 0;
    unsigned int b = 0;
    unsigned int c = 0;
    unsigned int d = 0;
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return false;
    }

    return a == 10 ||
           (a == 172 && b >= 16 && b <= 31) ||
           (a == 192 && b == 168);
}

static bool is_virtual_adapter(IP_ADAPTER_ADDRESSES* adapter) {
    if (adapter->FriendlyName != NULL) {
        std::wstring lower_name = adapter->FriendlyName;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        if (lower_name.find(L"hyper-v")    != std::wstring::npos ||
            lower_name.find(L"wsl")        != std::wstring::npos ||
            lower_name.find(L"loopback")   != std::wstring::npos ||
            lower_name.find(L"virtual")    != std::wstring::npos ||
            lower_name.find(L"vmware")     != std::wstring::npos ||
            lower_name.find(L"virtualbox") != std::wstring::npos ||
            lower_name.find(L"vethernet")  != std::wstring::npos) {
            return true;
        }
    }
    if (adapter->IfType == IF_TYPE_TUNNEL ||
        adapter->IfType == IF_TYPE_PPP    ||
        adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return true;
    }
    return false;
}

static std::string detect_lan_ipv4() {
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG buffer_size = 16 * 1024;
    std::vector<unsigned char> buffer(buffer_size);

    IP_ADAPTER_ADDRESSES* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(&buffer[0]);
    ULONG result = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buffer_size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buffer_size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(&buffer[0]);
        result = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buffer_size);
    }

    if (result != NO_ERROR) {
        return std::string();
    }

    std::string physical_fallback;
    for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != NULL; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }

        if (is_virtual_adapter(adapter)) {
            continue;
        }

        for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
             address != NULL;
             address = address->Next) {
            if (address->Address.lpSockaddr == NULL ||
                address->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }

            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
            char text[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &ipv4->sin_addr, text, sizeof(text)) == NULL) {
                continue;
            }

            std::string ip = text;
            if (ip.compare(0, 4, "127.") == 0 || ip.compare(0, 8, "169.254.") == 0) {
                continue;
            }

            if (is_private_ipv4(ip)) {
                return ip;
            }

            if (physical_fallback.empty()) {
                physical_fallback = ip;
            }
        }
    }

    if (!physical_fallback.empty()) {
        return physical_fallback;
    }

    std::string fallback;
    for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != NULL; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }

        for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
             address != NULL;
             address = address->Next) {
            if (address->Address.lpSockaddr == NULL ||
                address->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }

            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
            char text[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &ipv4->sin_addr, text, sizeof(text)) == NULL) {
                continue;
            }

            std::string ip = text;
            if (ip.compare(0, 4, "127.") == 0 || ip.compare(0, 8, "169.254.") == 0) {
                continue;
            }

            if (is_private_ipv4(ip)) {
                return ip;
            }

            if (fallback.empty()) {
                fallback = ip;
            }
        }
    }

    return fallback;
}

static std::string make_base_url(const std::string& host, int port) {
    if (host.empty()) {
        return std::string();
    }
    std::ostringstream url;
    url << "http://" << host << ':' << port;
    return url.str();
}

static std::string append_token_to_url(const std::string& base_url, const std::string& token) {
    if (base_url.empty() || token.empty()) {
        return base_url;
    }
    return base_url + "/?token=" + token;
}

static void open_browser_after_start(const std::string& url) {
    std::thread([url]() {
        Sleep(500);
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }).detach();
}

static void send_error(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(message, "text/plain; charset=utf-8");
}

static std::string bool_json(bool value) {
    return value ? "true" : "false";
}

static void append_optional_int_json(std::ostringstream& out, const char* key, const std::optional<int64_t>& value) {
    out << ",\"" << key << "\":";
    if (value.has_value()) {
        out << *value;
    } else {
        out << "null";
    }
}

static void append_optional_small_int_json(std::ostringstream& out, const char* key, const std::optional<int>& value) {
    out << ",\"" << key << "\":";
    if (value.has_value()) {
        out << *value;
    } else {
        out << "null";
    }
}

static std::string photo_tags_json(const std::vector<PhotoTag>& tags) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"tag\":\"" << json_escape(tags[i].tag) << "\""
            << ",\"probability\":" << tags[i].probability
            << ",\"threshold\":" << tags[i].threshold
            << ",\"predicted\":" << bool_json(tags[i].predicted)
            << ",\"derived\":" << bool_json(tags[i].derived)
            << "}";
    }
    out << "]";
    return out.str();
}

static std::string photo_json(const PhotoRecord& photo, const std::vector<PhotoTag>& tags = {}) {
    std::ostringstream out;
    out << "{"
        << "\"id\":" << photo.id
        << ",\"fileName\":\"" << json_escape(photo.file_name) << "\""
        << ",\"relativePath\":\"" << json_escape(photo.relative_path) << "\""
        << ",\"folderPath\":\"" << json_escape(photo.folder_path) << "\""
        << ",\"mediaType\":\"" << json_escape(photo.media_type) << "\""
        << ",\"sizeBytes\":" << photo.size_bytes;
    append_optional_int_json(out, "capturedAt", photo.captured_at);
    out << ",\"modifiedAt\":" << photo.modified_at;
    append_optional_small_int_json(out, "width", photo.width);
    append_optional_small_int_json(out, "height", photo.height);
    out << ",\"thumbnailStatus\":\"" << json_escape(photo.thumbnail_status) << "\""
        << ",\"thumbnailUrl\":\"/api/photos/" << photo.id << "/thumbnail\""
        << ",\"downloadUrl\":\"/download/" << url_encode(photo.relative_path) << "\""
        << ",\"isFavorite\":" << bool_json(photo.is_favorite)
        << ",\"tags\":" << photo_tags_json(tags)
        << "}";
    return out.str();
}

static std::string scan_status_json(const ScanStatus& status) {
    std::ostringstream out;
    out << "{"
        << "\"status\":\"" << json_escape(status.status) << "\""
        << ",\"startedAt\":" << status.started_at
        << ",\"finishedAt\":" << status.finished_at
        << ",\"totalSeen\":" << status.total_seen
        << ",\"totalIndexed\":" << status.total_indexed
        << ",\"totalUpdated\":" << status.total_updated
        << ",\"totalRemoved\":" << status.total_removed
        << ",\"errorMessage\":\"" << json_escape(status.error_message) << "\""
        << "}";
    return out.str();
}

static std::vector<std::string> split_csv(const std::string& value) {
    std::vector<std::string> result;
    size_t pos = 0;
    while (pos <= value.size()) {
        size_t comma = value.find(',', pos);
        std::string part = comma == std::string::npos ? value.substr(pos) : value.substr(pos, comma - pos);
        if (!part.empty()) {
            result.push_back(part);
        }
        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
    return result;
}

static TimelineQuery timeline_query_from_request(const httplib::Request& req) {
    TimelineQuery query;
    query.limit = 100;
    query.missing = false;
    if (req.has_param("limit")) {
        try { query.limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
    }
    if (req.has_param("cursor")) {
        query.cursor = req.get_param_value("cursor");
    }
    if (req.has_param("folder")) {
        query.folder_path = req.get_param_value("folder");
    }
    if (req.has_param("media")) {
        query.media_types = split_csv(req.get_param_value("media"));
    }
    if (req.has_param("favorite")) {
        std::string value = req.get_param_value("favorite");
        if (value == "1" || value == "true") {
            query.favorite = true;
        }
    }
    return query;
}

static PhotoSearchQuery photo_search_query_from_request(const httplib::Request& req) {
    PhotoSearchQuery query;
    query.limit = 80;
    query.missing = false;
    if (req.has_param("q")) {
        query.keyword = req.get_param_value("q");
    }
    if (req.has_param("limit")) {
        try { query.limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
    }
    if (req.has_param("folder")) {
        query.folder_path = req.get_param_value("folder");
    }
    if (req.has_param("media")) {
        query.media_types = split_csv(req.get_param_value("media"));
    }
    if (req.has_param("favorite")) {
        std::string value = req.get_param_value("favorite");
        if (value == "1" || value == "true") {
            query.favorite = true;
        }
    }
    return query;
}

static std::string timeline_json(
    const std::vector<PhotoRecord>& photos,
    int requested_limit,
    const PhotoRepository& repository,
    bool include_next_cursor = true) {
    std::ostringstream out;
    out << "{\"items\":[";
    for (size_t i = 0; i < photos.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << photo_json(photos[i], repository.listPhotoTags(photos[i].id));
    }
    out << "],\"nextCursor\":";
    if (include_next_cursor && requested_limit > 0 && photos.size() >= static_cast<size_t>(requested_limit)) {
        const PhotoRecord& last = photos.back();
        int64_t sort_time = last.captured_at.value_or(last.modified_at);
        out << "\"" << sort_time << ':' << last.id << "\"";
    } else {
        out << "null";
    }
    out << "}";
    return out.str();
}

static std::string folders_json(const std::vector<FolderRecord>& folders) {
    std::ostringstream out;
    out << "{\"items\":[";
    for (size_t i = 0; i < folders.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        const FolderRecord& folder = folders[i];
        out << "{"
            << "\"relativePath\":\"" << json_escape(folder.relative_path) << "\""
            << ",\"photoCount\":" << folder.photo_count
            << ",\"videoCount\":" << folder.video_count
            << ",\"rawCount\":" << folder.raw_count;
        append_optional_int_json(out, "latestCapturedAt", folder.latest_captured_at);
        append_optional_int_json(out, "latestModifiedAt", folder.latest_modified_at);
        out << ",\"indexedAt\":" << folder.indexed_at
            << "}";
    }
    out << "]}";
    return out.str();
}

static std::string tidy_report_json(const TidyReport& report) {
    std::ostringstream out;
    out << "{"
        << "\"total_photos\":" << report.total_photos
        << ",\"total_similar_groups\":" << report.total_similar_groups
        << ",\"total_delete_candidates\":" << report.total_delete_candidates
        << ",\"total_review\":" << report.total_review
        << ",\"total_keep\":" << report.total_keep
        << ",\"groups_by_type\":" << report.groups_by_type_json
        << ",\"missing_embeddings\":" << report.missing_embeddings
        << ",\"skipped_photos\":" << report.skipped_photos
        << ",\"average_group_size\":" << report.average_group_size
        << ",\"estimated_reclaimable_count\":" << report.estimated_reclaimable_count
        << ",\"conservative_mode\":" << bool_json(report.conservative_mode)
        << ",\"delete_decision_uses_labels\":" << bool_json(report.delete_decision_uses_labels)
        << ",\"tag_score_used\":" << bool_json(report.tag_score_used)
        << ",\"embedding_based_grouping\":" << bool_json(report.embedding_based_grouping)
        << ",\"hash_based_duplicate_detection\":" << bool_json(report.hash_based_duplicate_detection)
        << ",\"quality_based_recommendation\":" << bool_json(report.quality_based_recommendation)
        << "}";
    return out.str();
}

static std::string similar_groups_json(const std::vector<SimilarGroupRecord>& groups) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < groups.size(); ++i) {
        if (i != 0) out << ",";
        const SimilarGroupRecord& group = groups[i];
        out << "{"
            << "\"id\":\"" << json_escape(group.group_id) << "\""
            << ",\"similar_group_id\":\"" << json_escape(group.group_id) << "\""
            << ",\"type\":\"" << json_escape(group.group_type) << "\""
            << ",\"scene_id\":\"" << json_escape(group.scene_id) << "\""
            << ",\"photo_count\":" << group.photos.size()
            << ",\"cover_photo_id\":" << group.cover_photo_id
            << ",\"best_photo_id\":" << group.best_photo_id
            << ",\"confidence\":" << group.confidence
            << ",\"reason\":\"" << json_escape(group.reason) << "\""
            << ",\"recommended_keep\":" << group.keep_count
            << ",\"delete_candidates\":" << group.delete_candidate_count
            << ",\"summary\":{"
            << "\"keep_count\":" << group.keep_count
            << ",\"review_count\":" << group.review_count
            << ",\"delete_candidate_count\":" << group.delete_candidate_count
            << "},\"photos\":[";
        for (size_t j = 0; j < group.photos.size(); ++j) {
            if (j != 0) out << ",";
            const SimilarGroupPhotoRecord& photo = group.photos[j];
            out << "{"
                << "\"photo_id\":" << photo.photo_id
                << ",\"similarity_to_best\":" << photo.similarity_to_best
                << ",\"similarity\":" << photo.similarity_to_best
                << ",\"hash_distance_to_best\":" << photo.hash_distance_to_best
                << ",\"quality_score\":" << photo.quality_score
                << ",\"recommend\":\"" << json_escape(photo.recommendation) << "\""
                << ",\"recommendation\":\"" << json_escape(photo.recommendation) << "\""
                << ",\"reasons\":" << photo.reasons_json
                << "}";
        }
        out << "]}";
    }
    out << "]";
    return out.str();
}

static std::string delete_candidates_json(const std::vector<DeleteCandidateRecord>& candidates) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i != 0) out << ",";
        const DeleteCandidateRecord& candidate = candidates[i];
        out << "{"
            << "\"candidate_id\":\"" << json_escape(candidate.candidate_id) << "\""
            << ",\"photo_id\":" << candidate.photo_id
            << ",\"group_id\":\"" << json_escape(candidate.group_id) << "\""
            << ",\"similar_group_id\":\"" << json_escape(candidate.group_id) << "\""
            << ",\"matched_best_photo_id\":" << candidate.matched_best_photo_id
            << ",\"similarity_to_best\":" << candidate.similarity_to_best
            << ",\"quality_score\":" << candidate.quality_score
            << ",\"best_quality_score\":" << candidate.best_quality_score
            << ",\"safe_to_delete_score\":" << candidate.safe_to_delete_score
            << ",\"reason\":\"" << json_escape(candidate.reason) << "\""
            << ",\"requires_user_confirmation\":" << bool_json(candidate.requires_user_confirmation)
            << ",\"recommended_action\":\"delete_candidate\""
            << ",\"status\":\"" << json_escape(candidate.status) << "\""
            << "}";
    }
    out << "]";
    return out.str();
}

// ---------------------------------------------------------------------------
// Server entry point
// ---------------------------------------------------------------------------

int Server::run(const Options& options) {
    ensure_parent_directory(options.photo_db_path);
    SQLitePhotoRepository photo_repository(options.photo_db_path);
    photo_repository.initialize();

    PythonPhotoTaggerOptions tagger_options;
    tagger_options.python_exe = options.album_cv_python.empty()
        ? L"C:\\Users\\18361\\.conda\\envs\\album-cv\\python.exe"
        : options.album_cv_python;
    tagger_options.project_root = options.album_cv_root.empty()
        ? L"C:\\Code\\PythonCode\\album-python-cv-"
        : options.album_cv_root;
    tagger_options.device = options.album_cv_device.empty() ? L"cuda" : options.album_cv_device;
    tagger_options.log_path = L"C:\\Code\\CppCode\\local-file-share\\tagger-last.log";

    OnnxPhotoTaggerOptions onnx_options;
    onnx_options.model_path = options.album_cv_onnx.empty()
        ? (std::filesystem::path(L"C:\\Code\\CppCode\\local-file-share") /
           L"models" /
           L"dinov2_album_tagger_v3" /
           L"dinov2_album_tagger_v3.onnx").wstring()
        : options.album_cv_onnx;
    std::unique_ptr<PhotoTagger> photo_tagger = create_photo_tagger(onnx_options, tagger_options);
    PhotoService photo_service(options.share_dir, photo_repository, photo_tagger.get());

    httplib::Server server;
    server.set_payload_max_length(0);
    server.set_read_timeout(10 * 60, 0);
    server.set_write_timeout(10 * 60, 0);
    server.set_keep_alive_timeout(10 * 60);
    std::string lan_ip = detect_lan_ipv4();

    // Generate auth token (empty string when auth is disabled)
    std::string token;
    if (!options.no_auth) {
        token = options.auth_token.empty() ? generate_token() : options.auth_token;
    }

    // Build URLs
    std::string lan_base_url = make_base_url(lan_ip, options.port);
    std::string local_base_url = make_base_url("127.0.0.1", options.port);
    std::string frontend_lan_base_url = options.dev_mode ? make_base_url(lan_ip, 5173) : std::string();
    std::string frontend_local_base_url = options.dev_mode ? make_base_url("127.0.0.1", 5173) : std::string();

    // access_url passed to render functions: includes token so QR/copy-link work
    std::string access_url = options.dev_mode && !frontend_lan_base_url.empty()
        ? frontend_lan_base_url
        : append_token_to_url(lan_base_url, token);
    std::string local_url = options.dev_mode && !frontend_local_base_url.empty()
        ? frontend_local_base_url
        : append_token_to_url(local_base_url, token);

    // -----------------------------------------------------------------------
    // Pre-routing: security headers + CORS + authentication
    // -----------------------------------------------------------------------
    server.set_pre_routing_handler(
        [&](const httplib::Request& req, httplib::Response& res) {
            // 1. Security headers on ALL responses
            res.set_header("Referrer-Policy", "no-referrer");
            res.set_header("X-Content-Type-Options", "nosniff");
            res.set_header("Cache-Control", "no-store");

            // 2. CORS – only in dev mode, only for Vite dev server
            if (options.dev_mode) {
                std::string origin = req.get_header_value("Origin");
                if (origin == "http://127.0.0.1:5173" ||
                    origin == "http://localhost:5173") {
                    res.set_header("Access-Control-Allow-Origin", origin);
                    res.set_header("Vary", "Origin");
                }

                // Handle preflight
                if (req.method == "OPTIONS") {
                    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                    res.set_header("Access-Control-Allow-Headers", "Content-Type");
                    res.status = 204;
                    return httplib::Server::HandlerResponse::Handled;
                }
            }

            // 3. Authentication (skip when --no-auth)
            if (!options.no_auth) {
                // Auth entry points:
                // - GET /?token=xxx for the built-in backend page.
                // - GET /api/auth?token=xxx for the Vite dev UI proxy.
                if ((req.path == "/" || req.path == "/api/auth") && req.has_param("token")) {
                    std::string provided = req.get_param_value("token");
                    if (provided == token) {
                        res.set_header("Set-Cookie",
                            "lfs_token=" + token + "; Path=/; HttpOnly; SameSite=Strict");
                        if (req.path == "/api/auth") {
                            res.set_content("{\"status\":\"ok\"}", "application/json; charset=utf-8");
                        } else {
                            res.set_header("Location", "/");
                            res.status = 302;
                        }
                        return httplib::Server::HandlerResponse::Handled;
                    } else {
                        send_auth_denied(res);
                        return httplib::Server::HandlerResponse::Handled;
                    }
                }

                // All other requests: require valid cookie
                if (!has_valid_token_cookie(req, token)) {
                    send_auth_denied(res);
                    return httplib::Server::HandlerResponse::Handled;
                }
            }

            return httplib::Server::HandlerResponse::Unhandled;
        });

    // -----------------------------------------------------------------------
    // Route handlers
    // -----------------------------------------------------------------------

    server.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(render_directory_page(options.share_dir, options.share_dir, access_url),
                        "text/html; charset=utf-8");
    });

    server.Get("/api/list", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string request_path;
            if (req.has_param("path")) {
                request_path = req.get_param_value("path");
            }

            std::wstring target = resolve_request_path(options.share_dir, request_path);
            if (!is_directory(target)) {
                send_error(res, 404, "Directory not found");
                return;
            }

            int offset = 0;
            int limit = 1000;
            if (req.has_param("offset")) {
                try { offset = std::stoi(req.get_param_value("offset")); } catch (...) {}
            }
            if (req.has_param("limit")) {
                try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
            }
            if (offset < 0) offset = 0;
            if (limit <= 0) limit = 1000;

            res.set_content(render_directory_json(options.share_dir, target, access_url, offset, limit),
                            "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    server.Post("/api/photos/scan", [&](const httplib::Request&, httplib::Response& res) {
        ScanStatus status = photo_service.startScanAsync();
        res.set_content(scan_status_json(status), "application/json; charset=utf-8");
    });

    server.Get("/api/photos/scan/status", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(scan_status_json(photo_service.latestStatus()), "application/json; charset=utf-8");
    });

    server.Get("/api/photos/timeline", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            TimelineQuery query = timeline_query_from_request(req);
            std::vector<PhotoRecord> photos = photo_repository.listTimeline(query);
            res.set_content(timeline_json(photos, query.limit <= 0 ? 100 : std::min(query.limit, 500), photo_repository),
                            "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Get("/api/photos/search", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            PhotoSearchQuery query = photo_search_query_from_request(req);
            std::vector<PhotoRecord> photos = photo_repository.searchPhotos(query);
            int requested_limit = query.limit <= 0 ? 80 : std::min(query.limit, 200);
            res.set_content(timeline_json(photos, requested_limit, photo_repository, false),
                            "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Get("/api/photos/folders", [&](const httplib::Request&, httplib::Response& res) {
        try {
            res.set_content(folders_json(photo_repository.listFolders()), "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Post("/api/photos/tidy/rebuild", [&](const httplib::Request&, httplib::Response& res) {
        try {
            TidyReport report = TidyEngine(photo_repository).rebuild();
            res.set_content(tidy_report_json(report), "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Get("/api/photos/similar_groups", [&](const httplib::Request&, httplib::Response& res) {
        try {
            res.set_content(similar_groups_json(photo_repository.listSimilarGroups()), "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Get("/api/photos/delete_candidates", [&](const httplib::Request&, httplib::Response& res) {
        try {
            res.set_content(delete_candidates_json(photo_repository.listDeleteCandidates()), "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Post(R"(/api/photos/delete_candidates/([^/]+)/confirm)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            photo_repository.updateDeleteCandidateStatus(req.matches[1], "confirmed");
            res.set_content("{\"status\":\"confirmed\"}", "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Post(R"(/api/photos/delete_candidates/([^/]+)/reject)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            photo_repository.updateDeleteCandidateStatus(req.matches[1], "rejected");
            res.set_content("{\"status\":\"rejected\"}", "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Post(R"(/api/photos/(\d+)/favorite)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int64_t id = std::stoll(req.matches[1]);
            bool favorite = req.body.find("true") != std::string::npos || req.body.find("\"favorite\":1") != std::string::npos;
            photo_repository.toggleFavorite(id, favorite);
            res.set_content("{\"status\":\"ok\",\"favorite\":" + bool_json(favorite) + "}",
                            "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Delete(R"(/api/photos/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int64_t id = std::stoll(req.matches[1]);
            PhotoRecord photo = photo_repository.getPhoto(id);
            
            std::wstring target = resolve_request_path(options.share_dir, photo.relative_path);
            if (is_regular_file(target)) {
                std::error_code ec;
                std::filesystem::remove(target, ec);
            }
            
            if (!photo.thumbnail_path.empty()) {
                std::error_code ec;
                std::filesystem::remove(utf8_to_wide(photo.thumbnail_path), ec);
            }
            
            photo_repository.deletePhoto(id);
            
            res.set_content("{\"status\":\"ok\"}", "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Post("/api/photos/delete_batch", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::vector<std::string> ids_str = split_csv(req.body);
            int count = 0;
            for (const std::string& id_str : ids_str) {
                try {
                    int64_t id = std::stoll(id_str);
                    PhotoRecord photo = photo_repository.getPhoto(id);
                    
                    std::wstring target = resolve_request_path(options.share_dir, photo.relative_path);
                    if (is_regular_file(target)) {
                        std::error_code ec;
                        std::filesystem::remove(target, ec);
                    }
                    if (!photo.thumbnail_path.empty()) {
                        std::error_code ec;
                        std::filesystem::remove(utf8_to_wide(photo.thumbnail_path), ec);
                    }
                    photo_repository.deletePhoto(id);
                    count++;
                } catch (...) {
                    // Ignore errors for individual files (e.g. already deleted)
                }
            }
            res.set_content("{\"status\":\"ok\",\"deleted\":" + std::to_string(count) + "}", "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Get(R"(/api/photos/(\d+)/thumbnail)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            PhotoRecord photo = photo_repository.getPhoto(std::stoll(req.matches[1]));
            if (photo.media_type != "image" || photo.missing) {
                send_error(res, 404, "Thumbnail is not available");
                return;
            }

            std::wstring target = resolve_request_path(options.share_dir, photo.relative_path);
            if (!is_regular_file(target)) {
                send_error(res, 404, "File not found");
                return;
            }

            unsigned long long file_size = get_file_size(target);
            res.set_content_provider(
                static_cast<size_t>(file_size),
                guess_mime_type(target),
                [target](size_t offset, size_t length, httplib::DataSink& sink) {
                    FILE* file = NULL;
                    if (_wfopen_s(&file, target.c_str(), L"rb") != 0 || file == NULL) {
                        return false;
                    }
                    if (_fseeki64(file, static_cast<__int64>(offset), SEEK_SET) != 0) {
                        fclose(file);
                        return false;
                    }
                    char buffer[8192];
                    bool ok = true;
                    while (length > 0) {
                        size_t to_read = std::min(sizeof(buffer), length);
                        size_t read_count = fread(buffer, 1, to_read, file);
                        if (read_count == 0) {
                            break;
                        }
                        if (!sink.write(buffer, read_count)) {
                            ok = false;
                            break;
                        }
                        length -= read_count;
                    }
                    fclose(file);
                    return ok;
                });
        } catch (const std::exception& ex) {
            send_error(res, 500, ex.what());
        }
    });

    server.Get("/qr.svg", [&](const httplib::Request&, httplib::Response& res) {
        if (access_url.empty()) {
            send_error(res, 503, "LAN address is unavailable");
            return;
        }

        res.set_content(make_qr_svg(access_url), "image/svg+xml; charset=utf-8");
    });

    server.Get("/qr", [&](const httplib::Request&, httplib::Response& res) {
        if (access_url.empty()) {
            send_error(res, 503, "LAN address is unavailable");
            return;
        }

        std::ostringstream html;
        html
            << "<!doctype html><html lang=\"zh-CN\"><head>"
            << "<meta charset=\"utf-8\">"
            << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
            << "<title>LocalFileShare QR</title>"
            << "<style>*{box-sizing:border-box}body{margin:0;font-family:\"SF Pro Display\",\"Segoe UI\",Helvetica,Arial,sans-serif;background:radial-gradient(circle at 75% 12%,rgba(85,159,255,.25),transparent 30%),linear-gradient(135deg,#fbfcff,#eef3f8);color:#16181d;display:grid;place-items:center;min-height:100vh;padding:22px;text-align:center}"
            << "main{width:min(520px,100%);background:rgba(255,255,255,.78);border:1px solid rgba(25,31,44,.11);border-radius:32px;padding:26px;box-shadow:0 24px 70px rgba(21,30,48,.13);backdrop-filter:blur(24px)}h1{font-size:30px;line-height:1.08;margin:0 0 8px}.hint{color:#6b7280;margin:0 0 20px}.qr{display:grid;place-items:center;background:linear-gradient(180deg,#fff,#f7f9fc);border:1px solid rgba(25,31,44,.11);border-radius:26px;padding:20px}img{width:328px;height:328px;max-width:78vw;max-height:78vw;image-rendering:pixelated;border-radius:12px}.url{font-family:Consolas,\"SFMono-Regular\",monospace;margin-top:18px;word-break:break-all;color:#0f766e;background:rgba(255,255,255,.68);border:1px solid rgba(25,31,44,.11);border-radius:18px;padding:13px}</style>"
            << "</head><body><main><h1>&#25195;&#30721;&#35775;&#38382;</h1><p class=\"hint\">&#22312;&#21516;&#19968;&#23616;&#22495;&#32593;&#20869;&#25171;&#24320;&#20849;&#20139;&#25991;&#20214;</p><div class=\"qr\"><img src=\"/qr.svg?v=2\" alt=\"QR code\"></div><div class=\"url\">"
            << html_escape(access_url)
            << "</div></main></body></html>";
        res.set_content(html.str(), "text/html; charset=utf-8");
    });

    server.Get(R"(/browse/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::wstring target = resolve_request_path(options.share_dir, req.matches[1]);
            if (!is_directory(target)) {
                send_error(res, 404, "Directory not found");
                return;
            }

            res.set_content(render_directory_page(options.share_dir, target, access_url),
                            "text/html; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    server.Get(R"(/download/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::wstring target = resolve_request_path(options.share_dir, req.matches[1]);
            if (!is_regular_file(target)) {
                send_error(res, 404, "File not found");
                return;
            }

            unsigned long long file_size = get_file_size(target);
            if (sizeof(size_t) < 8 && file_size > 0xFFFFFFFFULL) {
                send_error(res, 500, "File is too large to download on a 32-bit server");
                return;
            }

            std::string filename = wide_to_utf8(target.substr(target.find_last_of(L"\\/") + 1));
            res.set_header("Content-Disposition",
                "attachment; filename=\"download\"; filename*=UTF-8''" + url_encode(filename));
            res.set_content_provider(
                static_cast<size_t>(file_size),
                guess_mime_type(target),
                [target](size_t offset, size_t length, httplib::DataSink& sink) {
                    FILE* file = NULL;
                    if (_wfopen_s(&file, target.c_str(), L"rb") != 0 || file == NULL) {
                        return false;
                    }

                    if (_fseeki64(file, static_cast<__int64>(offset), SEEK_SET) != 0) {
                        fclose(file);
                        return false;
                    }

                    char buffer[8192];
                    bool ok = true;
                    while (length > 0) {
                        size_t to_read = std::min(sizeof(buffer), length);
                        size_t read_count = fread(buffer, 1, to_read, file);
                        if (read_count == 0) {
                            break;
                        }
                        if (!sink.write(buffer, read_count)) {
                            ok = false;
                            break;
                        }
                        length -= read_count;
                    }

                    fclose(file);
                    return ok;
                });
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    server.Post("/api/upload", [&](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& content_reader) {
        if (!req.is_multipart_form_data()) {
            send_error(res, 400, "Expected multipart/form-data");
            return;
        }

        try {
            std::string request_path;
            if (req.has_param("path")) {
                request_path = req.get_param_value("path");
            }
            bool trigger_scan = true;
            if (req.has_param("scan")) {
                std::string scan_value = req.get_param_value("scan");
                trigger_scan = scan_value != "0" && scan_value != "false";
            }

            std::wstring target_dir = resolve_request_path(options.share_dir, request_path);
            if (!is_directory(target_dir)) {
                send_error(res, 404, "Directory not found");
                return;
            }

            bool upload_ok = true;
            std::string error_msg;
            FILE* current_file = NULL;

            content_reader(
                [&](const httplib::FormData& file) {
                    if (current_file) {
                        fclose(current_file);
                        current_file = NULL;
                    }

                    if (file.filename.empty()) {
                        return true; // Skip fields that are not files
                    }

                    std::wstring filename = utf8_to_wide(file.filename);
                    size_t slash = filename.find_last_of(L"\\/");
                    if (slash != std::wstring::npos) {
                        filename = filename.substr(slash + 1);
                    }
                    
                    if (filename.empty() || filename == L"." || filename == L"..") {
                        return true;
                    }

                    std::wstring current_file_path = join_path(target_dir, filename);
                    if (_wfopen_s(&current_file, current_file_path.c_str(), L"wb") != 0 || current_file == NULL) {
                        upload_ok = false;
                        error_msg = "Failed to open file for writing: " + file.filename;
                        return false;
                    }
                    return true;
                },
                [&](const char* data, size_t data_length) {
                    if (current_file) {
                        if (fwrite(data, 1, data_length, current_file) != data_length) {
                            upload_ok = false;
                            error_msg = "Write error during upload";
                            return false;
                        }
                    }
                    return true;
                }
            );

            if (current_file) {
                fclose(current_file);
            }

            if (!upload_ok) {
                send_error(res, 500, error_msg.empty() ? "Upload failed" : error_msg);
            } else {
                if (trigger_scan) {
                    photo_service.startScanAsync();
                }
                res.set_content("{\"status\":\"ok\"}", "application/json; charset=utf-8");
            }
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    // -----------------------------------------------------------------------
    // Startup output
    // -----------------------------------------------------------------------

    std::cout << "LocalFileShare started.\n"
              << "Shared directory: " << wide_to_utf8(options.share_dir) << "\n"
              << "Photo database: " << wide_to_utf8(options.photo_db_path) << "\n"
              << "Album CV tagger: " << (photo_tagger->available() ? "enabled" : "disabled") << "\n";

    if (!options.no_auth) {
        std::cout << "Access token: " << token << "\n";
    } else {
        std::cout << "Authentication: DISABLED\n";
    }

    std::cout << "Local URL: " << local_url << "\n"
              << "LAN URL:   "
              << (lan_base_url.empty()
                  ? "http://<your-lan-ip>:" + std::to_string(options.port)
                  : access_url)
              << "\n"
              << "QR page:   " << local_url << "\n"
              << "Press Ctrl+C to stop.\n";

    if (options.open_browser) {
        open_browser_after_start(local_url);
    }

    if (!server.listen(options.host.c_str(), options.port)) {
        std::cerr << "Failed to listen on " << options.host << ':' << options.port << "\n";
        return 1;
    }

    return 0;
}
