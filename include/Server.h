#pragma once
#include <string>

struct Options {
    std::wstring share_dir;
    int port;
    std::string host;
    bool open_browser;
    bool no_auth;
    bool dev_mode;

    Options() : share_dir(L"."), port(8080), host("0.0.0.0"), open_browser(true), no_auth(false), dev_mode(false) {}
};

class Server {
public:
    static int run(const Options& options);
};
