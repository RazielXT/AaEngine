#pragma once

#include <string>

std::wstring as_wstring(const std::string_view& str);

std::string as_string(const std::wstring_view& str);