#pragma once

#include <vector>
#include <map>
#include <string>
#include <d3d12.h>
#include "ShaderResources.h"

struct MaterialDepthState
{
	bool check = true;
	bool write = true;
};

struct MaterialBlendState
{
	bool alphaBlend = false;
};

struct TextureRef
{
	std::string id;
	std::string file;
};

using SamplerRef = SamplerInfo;

struct MaterialPipelineInfo
{
	std::string ps_ref;
	std::string vs_ref;

	MaterialDepthState depth;
	MaterialBlendState blend;
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
