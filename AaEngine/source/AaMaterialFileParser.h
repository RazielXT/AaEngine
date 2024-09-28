#pragma once

#include <vector>
#include <map>
#include <string>
#include <d3d12.h>

struct RS_DESC
{
	RS_DESC()
	{
		alpha_blend = 0;
		depth_check = 1;
		depth_write = 1;
		culling = 1;
	}

	int alpha_blend;
	int depth_check;
	int depth_write;
	int culling;
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
};

struct MaterialPipelineInfo
{
	std::string ps_ref;
	std::string vs_ref;

	RS_DESC renderStateDesc;
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
