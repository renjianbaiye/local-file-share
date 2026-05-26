#pragma once
#include <string>

struct Options {
    std::wstring share_dir;
    std::wstring photo_db_path;
    int port;
    std::string host;
    bool open_browser;
    bool no_auth;
    bool dev_mode;
    std::string auth_token;
    std::wstring album_cv_python;
    std::wstring album_cv_root;
    std::wstring album_cv_device;
    std::wstring album_cv_onnx;

    Options() : share_dir(L"."), photo_db_path(), port(8080), host("0.0.0.0"), open_browser(true), no_auth(false), dev_mode(false), auth_token(), album_cv_python(), album_cv_root(), album_cv_device(L"cuda"), album_cv_onnx() {}
};

class Server {
public:
    static int run(const Options& options);
};
