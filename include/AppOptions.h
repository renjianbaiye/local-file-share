#pragma once

#include "Server.h"

#include <string>

void print_usage();
bool parse_int(const std::wstring& value, int& out);
bool parse_options(int argc, wchar_t* argv[], Options& options, bool& help_requested);
std::wstring default_photo_db_path();
void fill_default_photo_db_path(Options& options);
void ensure_parent_directory(const std::wstring& file_path);
