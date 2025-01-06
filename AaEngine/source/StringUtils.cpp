#include "StringUtils.h"
#include <codecvt>

std::wstring as_wstring(const std::string& str)
{
	return std::wstring(str.begin(), str.end());
}

std::string as_string(const std::wstring& str)
{
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(str);
}

std::string as_string(const wchar_t* str)
{
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(str);
}
