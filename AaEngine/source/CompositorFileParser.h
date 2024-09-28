#pragma once

#include <map>
#include <string>
#include <vector>
#include <d3d12.h>

struct CompositorTexture
{
	std::string name;
	float width;
	float height;
	bool targetScale = false;
	std::vector<DXGI_FORMAT> formats;
	bool depthBuffer = false;
};

struct CompositorPass
{
	std::string name;
	std::string target;

	std::string render;
	std::string after;

	std::string material;
	std::vector<std::pair<std::string, UINT>> inputs;
};

struct CompositorInfo
{
	std::string name;
	std::vector<CompositorTexture> textures;
	std::vector<CompositorPass> passes;
};

namespace CompositorFileParser
{
	CompositorInfo parseFile(std::string path);
};
