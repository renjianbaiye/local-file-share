#ifndef _WIN32
#error "The current C++11 build targets Windows first. Cross-platform support will be added later."
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN //让 windows.h 少包含一些不常用内容，加快编译速度，也减少命名冲突

#include "httplib.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <stdio.h>

namespace {

struct Options {
    std::wstring share_dir;
    int port;
    std::string host;

    Options() : share_dir(L"."), port(8080), host("0.0.0.0") {}
};

struct FileEntry {
    std::wstring path;
    std::wstring name;
    bool is_directory;
    unsigned long long size;
};

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), NULL, 0, NULL, NULL);
    if (size <= 0) {
        return std::string();
    }

    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], size, NULL, NULL);
    return result;
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), NULL, 0);
    if (size <= 0) {
        return std::wstring();
    }

    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], size);
    return result;
}

void print_usage() {
    std::cout
        << "LocalFileShare\n"
        << "Usage:\n"
        << "  LocalFileShare --dir <path> [--port <port>] [--host <host>]\n"
        << "  LocalFileShare <path>\n\n"
        << "Options:\n"
        << "  --dir <path>    Directory to share\n"
        << "  --port <port>   HTTP port, default 8080\n"
        << "  --host <host>   Listen host, default 0.0.0.0\n"
        << "  --help          Show this help\n";
}

bool parse_int(const std::wstring& value, int& out) {
    wchar_t* end = NULL;
    long parsed = std::wcstol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != L'\0' || parsed <= 0 || parsed > 65535) {
        return false;
    }

    out = static_cast<int>(parsed);
    return true;
}

bool parse_options(int argc, wchar_t* argv[], Options& options, bool& help_requested) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--help" || arg == L"-h") {
            print_usage();
            help_requested = true;
            return false;
        }

        if (arg == L"--dir") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --dir\n";
                return false;
            }
            options.share_dir = argv[++i];
            continue;
        }

        if (arg == L"--port") {
            if (i + 1 >= argc || !parse_int(argv[i + 1], options.port)) {
                std::cerr << "Invalid value for --port\n";
                return false;
            }
            ++i;
            continue;
        }

        if (arg == L"--host") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --host\n";
                return false;
            }
            options.host = wide_to_utf8(argv[++i]);
            continue;
        }

        if (arg.size() >= 2 && arg[0] == L'-' && arg[1] == L'-') {
            std::cerr << "Unknown option: " << wide_to_utf8(arg) << "\n";
            return false;
        }

        options.share_dir = arg;
    }

    return true;
}

std::string html_escape(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        switch (input[i]) {
        case '&': output += "&amp;"; break;
        case '<': output += "&lt;"; break;
        case '>': output += "&gt;"; break;
        case '"': output += "&quot;"; break;
        case '\'': output += "&#39;"; break;
        default: output += input[i]; break;
        }
    }

    return output;
}

std::string url_encode(const std::string& input) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;

    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(input[i]);
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == '/') {
            encoded << static_cast<char>(ch);
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }

    return encoded.str();
}

std::string url_decode(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            std::string hex = input.substr(i + 1, 2);
            char* end = NULL;
            long value = std::strtol(hex.c_str(), &end, 16);
            if (end == hex.c_str() + 2) {
                output += static_cast<char>(value);
                i += 2;
                continue;
            }
        }

        output += input[i] == '+' ? ' ' : input[i];
    }

    return output;
}

std::wstring trim_trailing_slashes(const std::wstring& path) {
    if (path.size() <= 3 && path.size() >= 2 && path[1] == L':') {
        return path;
    }

    size_t end = path.size();
    while (end > 0 && (path[end - 1] == L'\\' || path[end - 1] == L'/')) {
        --end;
    }
    return path.substr(0, end);
}

std::wstring full_path(const std::wstring& path) {
    DWORD size = GetFullPathNameW(path.c_str(), 0, NULL, NULL);
    if (size == 0) {
        throw std::runtime_error("Failed to resolve path");
    }

    std::wstring buffer(size, L'\0');
    DWORD written = GetFullPathNameW(path.c_str(), size, &buffer[0], NULL);
    if (written == 0 || written >= size) {
        throw std::runtime_error("Failed to resolve path");
    }

    buffer.resize(written);
    return trim_trailing_slashes(buffer);
}

std::wstring lowercase_path(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool is_directory(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool is_regular_file(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool is_inside_root(const std::wstring& root, const std::wstring& target) {
    std::wstring normalized_root = lowercase_path(full_path(root));
    std::wstring normalized_target = lowercase_path(full_path(target));

    if (normalized_target == normalized_root) {
        return true;
    }

    if (!normalized_root.empty() && normalized_root[normalized_root.size() - 1] != L'\\') {
        normalized_root += L'\\';
    }

    return normalized_target.compare(0, normalized_root.size(), normalized_root) == 0;
}

std::wstring join_path(const std::wstring& left, const std::wstring& right) {
    if (right.empty()) {
        return left;
    }

    if (left.empty() || left[left.size() - 1] == L'\\' || left[left.size() - 1] == L'/') {
        return left + right;
    }

    return left + L"\\" + right;
}

std::wstring resolve_request_path(const std::wstring& root, const std::string& raw_path) {
    std::string decoded = url_decode(raw_path);
    while (!decoded.empty() && decoded[0] == '/') {
        decoded.erase(decoded.begin());
    }

    std::replace(decoded.begin(), decoded.end(), '/', '\\');

    std::wstring relative = utf8_to_wide(decoded);
    if (relative.find(L'\0') != std::wstring::npos ||
        relative.find(L':') != std::wstring::npos ||
        (relative.size() >= 2 && relative[0] == L'\\' && relative[1] == L'\\')) {
        throw std::runtime_error("Invalid path");
    }

    std::wstring target = full_path(join_path(root, relative));
    if (!is_inside_root(root, target)) {
        throw std::runtime_error("Path is outside shared directory");
    }

    return target;
}

std::string relative_url_path(const std::wstring& root, const std::wstring& target) {
    std::wstring normalized_root = full_path(root);
    std::wstring normalized_target = full_path(target);

    if (normalized_target.size() < normalized_root.size()) {
        return std::string();
    }

    std::wstring relative = normalized_target.substr(normalized_root.size());
    while (!relative.empty() && (relative[0] == L'\\' || relative[0] == L'/')) {
        relative.erase(relative.begin());
    }

    std::replace(relative.begin(), relative.end(), L'\\', L'/');
    return wide_to_utf8(relative);
}

unsigned long long make_file_size(const WIN32_FIND_DATAW& data) {
    ULARGE_INTEGER value;
    value.HighPart = data.nFileSizeHigh;
    value.LowPart = data.nFileSizeLow;
    return value.QuadPart;
}

std::vector<FileEntry> list_directory(const std::wstring& current) {
    std::vector<FileEntry> entries;
    std::wstring pattern = join_path(current, L"*");

    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return entries;
    }

    do {
        std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }

        bool directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        FileEntry entry;
        entry.name = name;
        entry.path = join_path(current, name);
        entry.is_directory = directory;
        entry.size = directory ? 0 : make_file_size(data);
        entries.push_back(entry);
    } while (FindNextFileW(handle, &data));

    FindClose(handle);
    return entries;
}

std::string file_size_text(unsigned long long bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit = 0;

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        ++unit;
    }

    std::ostringstream text;
    if (unit == 0) {
        text << static_cast<unsigned long long>(size) << ' ' << units[unit];
    } else {
        text << std::fixed << std::setprecision(1) << size << ' ' << units[unit];
    }
    return text.str();
}

std::wstring parent_path(const std::wstring& path) {
    std::wstring normalized = trim_trailing_slashes(path);
    size_t pos = normalized.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return normalized;
    }
    if (pos == 2 && normalized.size() >= 3 && normalized[1] == L':') {
        return normalized.substr(0, 3);
    }
    return normalized.substr(0, pos);
}

std::string render_directory_page(const std::wstring& root, const std::wstring& current) {
    std::vector<FileEntry> entries = list_directory(current);

    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.is_directory != b.is_directory) {
            return a.is_directory > b.is_directory;
        }
        return lowercase_path(a.name) < lowercase_path(b.name);
    });

    std::string current_relative = relative_url_path(root, current);
    std::ostringstream html;

    html
        << "<!doctype html><html lang=\"zh-CN\"><head>"
        << "<meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        << "<title>LocalFileShare</title>"
        << "<style>"
        << "body{margin:0;font-family:Segoe UI,Arial,sans-serif;background:#f6f7f9;color:#1f2933;}"
        << "main{max-width:960px;margin:0 auto;padding:24px 16px;}"
        << "h1{font-size:24px;margin:0 0 8px;}"
        << ".path{color:#667085;margin-bottom:20px;word-break:break-all;}"
        << ".list{background:white;border:1px solid #d9dee7;border-radius:8px;overflow:hidden;}"
        << ".row{display:grid;grid-template-columns:1fr auto;gap:12px;align-items:center;padding:12px 14px;border-top:1px solid #edf0f5;text-decoration:none;color:inherit;}"
        << ".row:first-child{border-top:0;}"
        << ".row:hover{background:#f1f5f9;}"
        << ".name{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}"
        << ".meta{color:#667085;font-size:13px;}"
        << ".empty{padding:28px 14px;color:#667085;}"
        << "@media(max-width:640px){main{padding:18px 10px}.row{grid-template-columns:1fr}.meta{margin-left:26px}}"
        << "</style></head><body><main>"
        << "<h1>LocalFileShare</h1>"
        << "<div class=\"path\">/" << html_escape(current_relative) << "</div>"
        << "<div class=\"list\">";

    if (lowercase_path(full_path(current)) != lowercase_path(full_path(root))) {
        std::string parent_relative = relative_url_path(root, parent_path(current));
        html << "<a class=\"row\" href=\"/browse/" << url_encode(parent_relative) << "\">"
             << "<span class=\"name\">[..] Parent directory</span><span class=\"meta\">folder</span></a>";
    }

    if (entries.empty()) {
        html << "<div class=\"empty\">This folder is empty.</div>";
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        const FileEntry& entry = entries[i];
        std::string name = wide_to_utf8(entry.name);
        std::string relative = relative_url_path(root, entry.path);
        std::string escaped_name = html_escape(name);

        if (entry.is_directory) {
            html << "<a class=\"row\" href=\"/browse/" << url_encode(relative) << "\">"
                 << "<span class=\"name\">[DIR] " << escaped_name << "</span>"
                 << "<span class=\"meta\">folder</span></a>";
        } else {
            html << "<a class=\"row\" href=\"/download/" << url_encode(relative) << "\">"
                 << "<span class=\"name\">[FILE] " << escaped_name << "</span>"
                 << "<span class=\"meta\">" << file_size_text(entry.size) << "</span></a>";
        }
    }

    html << "</div></main></body></html>";
    return html.str();
}

std::wstring extension_of(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return L"";
    }
    return lowercase_path(path.substr(dot));
}

std::string guess_mime_type(const std::wstring& path) {
    std::wstring ext = extension_of(path);

    if (ext == L".html" || ext == L".htm") return "text/html; charset=utf-8";
    if (ext == L".txt" || ext == L".log") return "text/plain; charset=utf-8";
    if (ext == L".css") return "text/css; charset=utf-8";
    if (ext == L".js") return "application/javascript; charset=utf-8";
    if (ext == L".json") return "application/json; charset=utf-8";
    if (ext == L".png") return "image/png";
    if (ext == L".jpg" || ext == L".jpeg") return "image/jpeg";
    if (ext == L".gif") return "image/gif";
    if (ext == L".webp") return "image/webp";
    if (ext == L".pdf") return "application/pdf";
    if (ext == L".mp4") return "video/mp4";
    if (ext == L".mp3") return "audio/mpeg";
    return "application/octet-stream";
}

unsigned long long get_file_size(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        throw std::runtime_error("Failed to read file size");
    }

    ULARGE_INTEGER value;
    value.HighPart = data.nFileSizeHigh;
    value.LowPart = data.nFileSizeLow;
    return value.QuadPart;
}

void send_error(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(message, "text/plain; charset=utf-8");
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    Options options;
    bool help_requested = false;
    if (!parse_options(argc, argv, options, help_requested)) {
        return help_requested ? 0 : 1;
    }

    try {
        options.share_dir = full_path(options.share_dir);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    if (!is_directory(options.share_dir)) {
        std::cerr << "Shared directory does not exist or is not a directory: "
                  << wide_to_utf8(options.share_dir) << "\n";
        return 1;
    }

    httplib::Server server;

    server.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(render_directory_page(options.share_dir, options.share_dir), "text/html; charset=utf-8");
    });

    server.Get(R"(/browse/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::wstring target = resolve_request_path(options.share_dir, req.matches[1]);
            if (!is_directory(target)) {
                send_error(res, 404, "Directory not found");
                return;
            }

            res.set_content(render_directory_page(options.share_dir, target), "text/html; charset=utf-8");
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
            std::string filename = wide_to_utf8(target.substr(target.find_last_of(L"\\/") + 1));
            res.set_header("Content-Disposition", "attachment; filename=\"download\"; filename*=UTF-8''" + url_encode(filename));
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

    std::cout << "LocalFileShare started.\n"
              << "Shared directory: " << wide_to_utf8(options.share_dir) << "\n"
              << "Local URL: http://127.0.0.1:" << options.port << "\n"
              << "LAN URL:   http://<your-lan-ip>:" << options.port << "\n"
              << "Press Ctrl+C to stop.\n";

    if (!server.listen(options.host.c_str(), options.port)) {
        std::cerr << "Failed to listen on " << options.host << ':' << options.port << "\n";
        return 1;
    }

    return 0;
}
