#pragma once

#include "imgui.h"

class TextureOverlaySection
{
public:
	void draw();

private:
	ImGuiTextFilter textureFilter;
};
