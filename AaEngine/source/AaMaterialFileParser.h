#pragma once

#include <vector>
#include <map>
#include <string>
#include <d3d12.h>

struct MaterialDepthState
{
	bool check = true;
	bool write = true;
};

struct TextureRef
{
	std::string id;
	std::string file;
};

struct SamplerRef
{
	UINT maxAnisotropy = 8;
	D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC;
	D3D12_TEXTURE_ADDRESS_MODE bordering = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
};

struct MaterialPipelineInfo
{
	std::string ps_ref;
	std::string vs_ref;

	MaterialDepthState depth;
	D3D12_CULL_MODE culling = D3D12_CULL_MODE_BACK;
};

struct MaterialResourcesInfo
{
	std::vector<TextureRef> textures;
	std::vector<SamplerRef> samplers;
	std::vector<std::string> uavs;

	std::map<std::string, std::vector<float>> defaultParams;
};

struct MaterialRef
{
	std::string base;
	bool abstract = false;
	std::string name;

	MaterialPipelineInfo pipeline;
	MaterialResourcesInfo resources;
};

namespace AaMaterialFileParser
{
	void parseAllMaterialFiles(std::vector<MaterialRef>& mats, std::string directory, bool subFolders = false);
};
