#ifndef _WIN32
#error "The current C++11 build targets Windows first. Cross-platform support will be added later."
#endif

#include "AppOptions.h"
#include "Server.h"
#include "FileManager.h"

#include <iostream>

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
        fill_default_photo_db_path(options);
        options.photo_db_path = full_path(options.photo_db_path);
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
