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
	UINT arraySize = 1;
	bool uav = false;
};

struct CompositorTextureInput
{
	std::string name;
	UINT index{};
	enum { Full, Index, Depth } type = Full;
};

struct CompositorPassInfo
{
	std::string name;
	std::string target;

	static const UINT AllTargets = -1;
	UINT targetIndex = AllTargets;

	std::string task;
	std::string after;

	std::string material;
	std::vector<CompositorTextureInput> inputs;
	std::vector<std::string> params;
};

struct CompositorInfo
{
	std::string name;
	std::map<std::string, CompositorTextureInfo> textures;
	std::vector<CompositorPassInfo> passes;
};

namespace CompositorFileParser
{
	CompositorInfo parseFile(std::string directory, std::string path);
};
