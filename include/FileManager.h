#pragma once
#include <string>
#include <vector>

struct FileEntry {
    std::wstring path;
    std::wstring name;
    bool is_directory;
    unsigned long long size;
};

std::string wide_to_utf8(const std::wstring& value);
std::wstring utf8_to_wide(const std::string& value);

std::wstring trim_trailing_slashes(const std::wstring& path);
std::wstring full_path(const std::wstring& path);
std::wstring lowercase_path(std::wstring value);
bool is_directory(const std::wstring& path);
bool is_regular_file(const std::wstring& path);
bool is_inside_root(const std::wstring& root, const std::wstring& target);
std::wstring join_path(const std::wstring& left, const std::wstring& right);
std::wstring resolve_request_path(const std::wstring& root, const std::string& raw_path);
std::string relative_url_path(const std::wstring& root, const std::wstring& target);
std::wstring parent_path(const std::wstring& path);
std::wstring extension_of(const std::wstring& path);
unsigned long long get_file_size(const std::wstring& path);
std::vector<FileEntry> list_directory(const std::wstring& current);
