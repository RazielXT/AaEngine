#pragma once

#include "AaShaderCompiler.h"
#include "ResourcesManager.h"
#include "ShaderConstantBuffers.h"

struct LoadedShader;
struct ShaderTextureView;

struct SignatureInfo
{
	struct CBuffer
	{
		CBufferInfo& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<CBuffer> cbuffers;

	struct Texture
	{
		TextureInfo& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<Texture> textures;

	struct Sampler
	{
		SamplerInfo& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<Sampler> samplers;

	CBuffer* rootBuffer = nullptr;
	UINT textureTargets{};
	bool hasVertexInput = false;
	bool bindlessTextures = false;

	void add(LoadedShader* shader, ShaderType type);
	void finish();

private:

	D3D12_SHADER_VISIBILITY getVisibility(ShaderType t);
	void addVisibility(D3D12_SHADER_VISIBILITY& v, ShaderType t);
};

constexpr const UINT BindlessTextureIndex = -1;

struct ResourcesInfo
{
	struct CBuffer
	{
		std::vector<float> defaultData;
		bool rootConstants = false;
		UINT rootIndex{};
		CbufferView globalBuffer;
	};
	std::vector<CBuffer> cbuffers;

	struct Texture
	{
		ShaderTextureView* texture{};
		UINT rootIndex = BindlessTextureIndex;
	};
	std::vector<Texture> textures;

	enum class AutoParam
	{
		None,
		WVP_MATRIX,
		VP_MATRIX,
		WORLD_MATRIX,
		SHADOW_MATRIX,
		TEXID,

		SUN_DIRECTION,
		TIME,
		VIEWPORT_SIZE_INV,
	};
	struct AutoParamInfo
	{
		AutoParam type;
		UINT bufferIdx;
		UINT bufferOffset;
	};
	std::vector<AutoParamInfo> autoParams;
};

struct FrameGpuParameters
{
	XMFLOAT3 sunDirection;
	XMFLOAT2 inverseViewportSize;
	float time;
	XMFLOAT4X4 shadowMapViewProjectionTransposed;
};
