#pragma once
#include <string>

std::string html_escape(const std::string& input);
std::string json_escape(const std::string& input);
std::string url_encode(const std::string& input);
std::string url_decode(const std::string& input);

std::string file_size_text(unsigned long long bytes);

std::string render_directory_page(const std::wstring& root, const std::wstring& current, const std::string& access_url);
std::string render_directory_json(const std::wstring& root, const std::wstring& current, const std::string& access_url, int offset = 0, int limit = 1000);
std::string make_qr_svg(const std::string& value);
std::string guess_mime_type(const std::wstring& path);
