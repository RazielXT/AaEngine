#pragma once

#include <string>

std::wstring as_wstring(const std::string& str);

std::string as_string(const wchar_t* str);
std::string as_string(const std::wstring& str);