#pragma once

#include "Editor/EntityPicker.h"

#include <functional>
#include <string>

class Camera;
class ViewportTool;

struct ViewportPlacementRequest
{
	ViewportTool* owner = nullptr;
	std::string hint;
	std::function<void(const EntityPicker::PickInfo&, Camera&)> place;
	std::function<void()> cancel;
};