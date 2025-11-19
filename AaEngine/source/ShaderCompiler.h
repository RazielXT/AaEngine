#pragma once

#include "RenderSystem.h"
#include "ShaderFileParser.h"
#include <dxcapi.h>
#include "directx\d3dx12.h"
#include <unordered_set>

namespace ShaderReflection
{
	struct CBuffer
	{
		std::string Name;
		UINT Size;
		UINT Slot;
		UINT Space;

		struct Parameter
		{
			std::string Name;
			UINT StartOffset;
			UINT Size;
		};
		std::vector<Parameter> Params;
	};

	struct Texture
	{
		std::string Name;
		UINT Slot;
		UINT Space;
	};

	struct Sampler
	{
		std::string Name;
		UINT Slot;
	};

	struct UAV
	{
		std::string Name;
		UINT Slot;
		UINT Space;
	};

	struct StructuredBuffer
	{
		std::string Name;
		UINT Slot;
		UINT Space;
	};
};

struct ShaderDescription
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	size_t outputTargets = 0;
	std::vector<ShaderReflection::CBuffer> cbuffers;
	std::vector<ShaderReflection::Texture> textures;
	std::vector<ShaderReflection::Sampler> samplers;
	std::vector<ShaderReflection::UAV> uavs;
	std::vector<ShaderReflection::StructuredBuffer> structuredBuffers;
	std::vector<ShaderReflection::StructuredBuffer> rwStructuredBuffers;

	std::vector<std::string> compiledIncludes;
	std::unordered_set<std::string> allDefines;

	bool bindlessTextures = false;
};

class ShaderCompiler
{
public:

	ShaderCompiler();
	~ShaderCompiler();

	ComPtr<IDxcBlob> compileShader(const ShaderRef& ref, ShaderDescription&, ShaderType);

private:

	bool reflectShaderInfo(IDxcResult* compiledShaderBuffer, ShaderDescription&, ShaderType);

	ComPtr<IDxcUtils> pUtils;
	ComPtr<IDxcCompiler3> pCompiler;
};
