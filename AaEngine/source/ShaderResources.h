#pragma once

#include "ShaderConstantsProvider.h"
#include <DirectXMath.h>
#include <array>

struct ShaderTextureView;
class ShaderUAV;

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
	Global,
	Root,
	Instancing,
	Geometry,
	RWBuffer,
	COUNT
};

struct ResourcesInfo
{
	struct GpuBuffer
	{
		std::vector<float> defaultData;
		GpuBufferType type{};
		UINT rootIndex{};
		CbufferView globalCBuffer;
	};
	std::vector<GpuBuffer> buffers;

	struct Texture
	{
		ShaderTextureView* texture{};
		UINT rootIndex = BindlessTextureIndex;
	};
	std::vector<Texture> textures;

	struct UAV
	{
		ShaderUAV* uav{};
		UINT rootIndex{};
	};
	std::vector<UAV> uavs;

	enum class AutoParam
	{
		None,
		WVP_MATRIX,
		VP_MATRIX,
		INV_VP_MATRIX,
		WORLD_MATRIX,
		TEXID,

		SHADOW_MATRIX,
		SHADOW_MAP_SIZE,
		SHADOW_MAP_SIZE_INV,
		SUN_DIRECTION,
		TIME,
		VIEWPORT_SIZE_INV,
		CAMERA_POSITION,
		WORLD_POSITION,
	};
	struct AutoParamInfo
	{
		AutoParam type;
		UINT bufferIdx;
		UINT bufferOffset;
	};
	std::vector<AutoParamInfo> autoParams;
};

enum class FastParam
{
	MaterialColor,
	TexIdDiffuse,
	Emission,
	COUNT
};

constexpr std::array<std::string_view, (int)FastParam::COUNT> FastParamNames =
{
	"MaterialColor",
	"TexIdDiffuse",
	"Emission",
};

struct PssmParameters
{
	XMFLOAT4X4 ShadowMatrix[2];
	XMFLOAT3 SunDirection;
	UINT TexIdShadowOffsetStart;
	float ShadowMapSize;
	float ShadowMapSizeInv;
};

struct FrameParameters
{
	PssmParameters sun;

	float time;
	float timeDelta;
};
