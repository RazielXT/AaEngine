#pragma once

#include <vector>
#include <map>
#include <string>
#include <d3d12.h>
#include "ShaderResources.h"
#include <optional>

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
	std::string compositorId;
};

using SamplerRef = SamplerInfo;

struct MaterialPipelineInfo
{
	std::string ps_ref;
	std::string vs_ref;

	MaterialDepthState depth;
	MaterialBlendState blend;
	D3D12_CULL_MODE culling = D3D12_CULL_MODE_BACK;
	D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID;
	float slopeScaledDepthBias = 0.0f;
	int depthBias = 0;
};

struct MaterialResourcesInfo
{
	std::vector<TextureRef> textures;
	std::vector<SamplerRef> samplers;
	std::vector<std::string> uavs;

	std::map<std::string, std::vector<float>> defaultParams;
};

enum class MaterialTechnique
{
	Default,
	Depth,
	DepthShadowmap,
	Voxelize,
	EntityId,
	TerrainScan,
	Wireframe,
	COUNT
};

struct MaterialRef
{
	std::string base;
	bool abstract = false;
	std::string name;

	MaterialPipelineInfo pipeline;
	MaterialResourcesInfo resources;

	std::array<std::optional<std::string>, int(MaterialTechnique::COUNT)> techniqueMaterial;

	struct TechniqueOverride
	{
		MaterialTechnique technique;
		std::string overrideMaterial;
	};
	std::vector<TechniqueOverride> techniqueOverrides;
};

struct shaderRefMaps;

namespace MaterialFileParser
{
	void parseAllMaterialFiles(std::vector<MaterialRef>& mats, shaderRefMaps& shaders, std::string directory, bool subFolders = false);
};
