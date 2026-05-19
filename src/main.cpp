#ifndef _WIN32
#error "The current C++11 build targets Windows first. Cross-platform support will be added later."
#endif

#include "Server.h"
#include "FileManager.h"

#include <iostream>
#include <cwctype>
#include <string>

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
        << "  --no-open       Do not open the browser automatically\n"
        << "  --no-auth       Disable token authentication (only allowed with 127.0.0.1/localhost)\n"
        << "  --dev           Dev mode: allow CORS from Vite dev server (127.0.0.1:5173)\n"
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

int wmain(int argc, wchar_t* argv[]) {
    Options options;
    bool help_requested = false;
    if (!parse_options(argc, argv, options, help_requested)) {
        return help_requested ? 0 : 1;
    }

    // --no-auth safety check: only allowed when listening on localhost
    if (options.no_auth &&
        options.host != "127.0.0.1" &&
        options.host != "localhost") {
        std::cerr << "Error: --no-auth is only allowed when --host is 127.0.0.1 or localhost.\n"
                  << "Listening on " << options.host << " without authentication is dangerous.\n"
                  << "Anyone on the same network could access your files.\n";
        return 1;
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

    return Server::run(options);
}
