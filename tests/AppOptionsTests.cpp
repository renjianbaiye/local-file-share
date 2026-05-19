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
