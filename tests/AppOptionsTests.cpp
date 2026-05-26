#include "AppOptions.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static wchar_t* mutable_arg(const wchar_t* value) {
    return const_cast<wchar_t*>(value);
}

int main() {
    Options parsed;
    bool help_requested = false;
    wchar_t* argv[] = {
        mutable_arg(L"LocalFileShare.exe"),
        mutable_arg(L"--photo-db"),
        mutable_arg(L"D:\\tmp\\album.db")
    };

    assert(parse_options(3, argv, parsed, help_requested));
    assert(!help_requested);
    assert(parsed.photo_db_path == L"D:\\tmp\\album.db");

    Options token_parsed;
    bool token_help_requested = false;
    wchar_t* token_argv[] = {
        mutable_arg(L"LocalFileShare.exe"),
        mutable_arg(L"--token"),
        mutable_arg(L"abc123")
    };

    assert(parse_options(3, token_argv, token_parsed, token_help_requested));
    assert(!token_help_requested);
    assert(token_parsed.auth_token == "abc123");

    Options onnx_parsed;
    bool onnx_help_requested = false;
    wchar_t* onnx_argv[] = {
        mutable_arg(L"LocalFileShare.exe"),
        mutable_arg(L"--album-cv-onnx"),
        mutable_arg(L"D:\\models\\dinov2_album_tagger_v3.onnx")
    };

    assert(parse_options(3, onnx_argv, onnx_parsed, onnx_help_requested));
    assert(!onnx_help_requested);
    assert(onnx_parsed.album_cv_onnx == L"D:\\models\\dinov2_album_tagger_v3.onnx");

    std::filesystem::path temp_root = std::filesystem::temp_directory_path() / L"lfs-app-options-test";
    std::filesystem::remove_all(temp_root);
    SetEnvironmentVariableW(L"LOCALAPPDATA", temp_root.wstring().c_str());

    std::wstring expected = (temp_root / L"LocalFileShare" / L"photos.db").wstring();
    assert(default_photo_db_path() == expected);

    Options defaults;
    fill_default_photo_db_path(defaults);
    assert(defaults.photo_db_path == expected);

    ensure_parent_directory(defaults.photo_db_path);
    assert(std::filesystem::is_directory(temp_root / L"LocalFileShare"));

    std::filesystem::remove_all(temp_root);
    return 0;
}
