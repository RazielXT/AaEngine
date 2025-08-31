#include "StringUtils.h"
#include <windows.h>

std::wstring as_wstring(const std::string_view& str)
{
	return std::wstring(str.begin(), str.end());
}

std::string as_string(const std::wstring_view& wstr)
{
	if (wstr.empty())
		return {};

	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
	std::string str(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &str[0], sizeNeeded, nullptr, nullptr);

	return str;
}
