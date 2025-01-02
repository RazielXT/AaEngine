#include "StringUtils.h"

std::wstring as_wstring(const std::string& str)
{
	return std::wstring(str.begin(), str.end());
}
