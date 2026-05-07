#pragma once

#include "Directx.h"
#include <string>

DXGI_FORMAT StringToDxgiFormat(const std::string& str);
const char* DxgiFormatToString(DXGI_FORMAT fmt);
