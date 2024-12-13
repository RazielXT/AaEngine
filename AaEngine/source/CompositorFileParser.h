#pragma once

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <d3d12.h>

namespace Compositor
{
	enum UsageFlags { PixelShader = 1, ComputeShader = 2, DepthRead = 4, Read = 8 };
};

struct CompositorTextureInfo
{
	std::string name;
	float width{};
	float height{};
	bool targetScale = false;
	bool outputScale = false;
	DXGI_FORMAT format{};
	UINT arraySize = 1;
	bool uav = false;
};

struct CompositorTextureSlot
{
	std::string name;

	Compositor::UsageFlags flags = Compositor::PixelShader;
};

struct CompositorPassCondition
{
	bool accept;
	std::string param;

	bool operator==(const CompositorPassCondition& other) const { return (accept == other.accept) && (param == other.param); }
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
	std::optional<CompositorPassCondition> condition;
};

struct CompositorInfo
{
	std::string name;
	std::map<std::string, CompositorTextureInfo> textures;
	std::vector<CompositorPassInfo> passes;
	std::map<std::string, std::vector<std::string>> mrt;
};

namespace CompositorFileParser
{
	CompositorInfo parseFile(std::string directory, std::string path);
};
