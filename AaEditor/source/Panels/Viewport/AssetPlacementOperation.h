#pragma once

#include "ViewportPlacement.h"

#include <string>

class ApplicationCore;

ViewportPlacementRequest createAssetPlacementRequest(ApplicationCore& app, std::string assetPath);