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
	DXGI_FORMAT format;
};

struct CompositorPass
{
	std::string name;
	std::string target;

	bool earlyZ = false;
	std::string render;

	std::string material;
	std::vector<std::string> inputs;
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
