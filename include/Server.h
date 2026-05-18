#pragma once
#include <string>

struct Options {
    std::wstring share_dir;
    int port;
    std::string host;
    bool open_browser;

    Options() : share_dir(L"."), port(8080), host("0.0.0.0"), open_browser(true) {}
};

class Server {
public:
    static int run(const Options& options);
};
