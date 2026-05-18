#include "FileManager.h"
#include "HtmlRenderer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <stdexcept>

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

std::wstring extension_of(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return L"";
    }
    return lowercase_path(path.substr(dot));
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

static unsigned long long make_file_size(const WIN32_FIND_DATAW& data) {
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
