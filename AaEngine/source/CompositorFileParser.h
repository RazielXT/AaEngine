#pragma once

#include <map>
#include <string>
#include <vector>
#include <d3d12.h>

struct CompositorTextureInfo
{
	std::string name;
	float width;
	float height;
	bool targetScale = false;
	std::vector<DXGI_FORMAT> formats;
	bool depthBuffer = false;
};

struct CompositorPassInfo
{
	std::string name;
	std::string target;

	static const UINT AllTargets = -1;
	UINT targetIndex = AllTargets;

	std::string render;
	std::string after;

	std::string material;
	std::vector<std::pair<std::string, UINT>> inputs;
	std::vector<std::string> params;
};

struct CompositorInfo
{
	std::string name;
	std::vector<CompositorTextureInfo> textures;
	std::vector<CompositorPassInfo> passes;
};

namespace CompositorFileParser
{
	CompositorInfo parseFile(std::string path);
};
