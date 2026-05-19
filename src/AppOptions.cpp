#include "AppOptions.h"

#include "FileManager.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cwctype>
#include <filesystem>
#include <iostream>
#include <stdexcept>

void print_usage() {
    std::cout
        << "LocalFileShare\n"
        << "Usage:\n"
        << "  LocalFileShare --dir <path> [--port <port>] [--host <host>]\n"
        << "  LocalFileShare <path>\n\n"
        << "Options:\n"
        << "  --dir <path>       Directory to share\n"
        << "  --port <port>      HTTP port, default 8080\n"
        << "  --host <host>      Listen host, default 0.0.0.0\n"
        << "  --photo-db <path>  SQLite photo album database path\n"
        << "  --token <token>    Use a specific access token instead of generating one\n"
        << "  --no-open          Do not open the browser automatically\n"
        << "  --no-auth          Disable token authentication (only allowed with 127.0.0.1/localhost)\n"
        << "  --dev              Dev mode: allow CORS from Vite dev server (127.0.0.1:5173)\n"
        << "  --help             Show this help\n";
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

        if (arg == L"--photo-db") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --photo-db\n";
                return false;
            }
            options.photo_db_path = argv[++i];
            continue;
        }

        if (arg == L"--token") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --token\n";
                return false;
            }
            options.auth_token = wide_to_utf8(argv[++i]);
            continue;
        }

        if (arg == L"--no-open") {
            options.open_browser = false;
            continue;
        }

        if (arg == L"--no-auth") {
            options.no_auth = true;
            continue;
        }

        if (arg == L"--dev") {
            options.dev_mode = true;
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

std::wstring default_photo_db_path() {
    DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", NULL, 0);
    if (size == 0) {
        throw std::runtime_error("LOCALAPPDATA is not set");
    }

    std::wstring local_app_data(size, L'\0');
    DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", &local_app_data[0], size);
    if (written == 0 || written >= size) {
        throw std::runtime_error("Failed to read LOCALAPPDATA");
    }
    local_app_data.resize(written);

    std::filesystem::path path(local_app_data);
    path /= L"LocalFileShare";
    path /= L"photos.db";
    return path.wstring();
}

void fill_default_photo_db_path(Options& options) {
    if (options.photo_db_path.empty()) {
        options.photo_db_path = default_photo_db_path();
    }
}

void ensure_parent_directory(const std::wstring& file_path) {
    std::filesystem::path parent = std::filesystem::path(file_path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}
