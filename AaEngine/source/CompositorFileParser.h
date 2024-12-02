#pragma once

#include <map>
#include <string>
#include <vector>
#include <d3d12.h>

namespace Compositor
{
	enum UsageFlags { PixelShader = 1, ComputeShader = 2, DepthRead = 4 };
};

struct CompositorTextureInfo
{
	std::string name;
	float width{};
	float height{};
	bool targetScale = false;
	DXGI_FORMAT format{};
	UINT arraySize = 1;
	bool uav = false;
};

struct CompositorTextureSlot
{
	std::string name;

	Compositor::UsageFlags flags = Compositor::PixelShader;
};
struct CompositorPassInfo
{
	std::string name;
	std::string task;
	std::string after;

	std::vector<CompositorTextureSlot> inputs;
	std::vector<CompositorTextureSlot> targets;

	std::string material;
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
