#pragma once

#include "ShaderConstantsProvider.h"
#include <DirectXMath.h>
#include <array>

struct ShaderTextureView;
class ShaderTextureViewUAV;

constexpr const UINT BindlessTextureIndex = -1;

struct SamplerInfo
{
	UINT maxAnisotropy = 8;
	D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC;
	D3D12_TEXTURE_ADDRESS_MODE bordering = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
};

enum class GpuBufferType
{
	None,
	Global,
	Root,
	Instancing,
	Geometry,
	COUNT
};

constexpr UINT HashConstexpr(std::string_view s)
{
	UINT hash = 2166136261u;
	for (char c : s)
	{
		hash ^= static_cast<UINT>(c);
		hash *= 16777619u;
	}
	return hash;
}

struct ParamId
{
	UINT value;

	constexpr ParamId(const char* name) : value(HashConstexpr(name)) {}

	bool operator==(const ParamId&) const = default;
};

struct ResourcesInfo
{
	struct RootGpuBuffer
	{
		std::vector<float> defaultData;
		UINT rootIndex{};
	};
	RootGpuBuffer rootBuffer;

	struct GpuBuffer
	{
		GpuBufferType type{};
		UINT rootIndex{};
		CbufferView globalCBuffer;
	};
	std::vector<GpuBuffer> buffers;

	struct Texture
	{
		ShaderTextureView* texture{};
		UINT rootIndex = BindlessTextureIndex;
		UINT autoParamIdx = {};
	};
	std::vector<Texture> textures;
	UINT boundTexturesCount{};

	struct UAV
	{
		ID3D12Resource* uav;
		UINT rootIndex{};
	};
	std::vector<UAV> uavs;

	enum class AutoParam
	{
		None,

		TEXID,

		WORLD_MATRIX,
		INV_WORLD_MATRIX,
		PREV_WORLD_MATRIX,
		WORLD_POSITION,

		WVP_MATRIX,
		VP_MATRIX,
		VIEW_MATRIX,
		PROJ_MATRIX,
		INV_VP_MATRIX,
		INV_VIEW_MATRIX,
		INV_PROJ_MATRIX,
		Z_MAGIC,
		SHADOW_MATRIX,
		SHADOW_MAP_SIZE,
		SHADOW_MAP_SIZE_INV,
		SUN_DIRECTION,
		SUN_COLOR,
		TIME,
		DELTA_TIME,
		VIEWPORT_SIZE_INV,
		VIEWPORT_SIZE,
		CAMERA_POSITION,
	};
	struct AutoParamInfo
	{
		AutoParam type;
		UINT bufferOffset;
	};
	std::vector<AutoParamInfo> frameAutoParams;
	std::vector<AutoParamInfo> objectAutoParams;
	std::vector<AutoParamInfo> resourceAutoParams;

	struct ParamInfo
	{
		ParamId id;
		UINT size{};
		UINT offset{};
	};
	std::vector<ParamInfo> params{};
};

struct PssmParameters
{
	XMFLOAT4X4 ShadowMatrix[4];
	XMFLOAT4X4 MaxShadowMatrix;
	XMFLOAT3 SunDirection;
	UINT TexIdShadowOffsetStart;
	XMFLOAT3 SunColor;
	float ShadowMapSize;
	float ShadowMapSizeInv;
	float ShadowCascadeDistance[3];
};

struct FrameParameters
{
	PssmParameters sun;

	float time{};
	float timeDelta{};
	UINT frameIndex{};
};
