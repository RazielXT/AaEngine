#pragma once

#include "AaRenderSystem.h"
#include "AaShaderFileParser.h"
#include <dxcapi.h>
#include "directx\d3dx12.h"

struct CBufferInfo
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

struct TextureInfo
{
	std::string Name;
	UINT Slot;
	UINT Space;
};

struct SamplerInfo
{
	std::string Name;
	UINT Slot;
};

struct UAVInfo
{
	std::string Name;
	UINT Slot;
	UINT Space;
};

struct ShaderDescription
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	size_t outputTargets = 0;
	std::vector<CBufferInfo> cbuffers;
	std::vector<TextureInfo> textures;
	std::vector<SamplerInfo> samplers;
	std::vector<UAVInfo> uavs;

	bool bindlessTextures = false;
};

class AaShaderCompiler
{
public:

	AaShaderCompiler();
	~AaShaderCompiler();

	ComPtr<IDxcBlob> compileShader(const ShaderRef& ref, ShaderDescription&, ShaderType);

private:

	bool reflectShaderInfo(IDxcResult* compiledShaderBuffer, ShaderDescription&, ShaderType);

	ComPtr<IDxcUtils> pUtils;
	ComPtr<IDxcCompiler3> pCompiler;
};
