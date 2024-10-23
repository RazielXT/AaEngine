#pragma once

#include "ShaderConstants.h"
#include <DirectXMath.h>

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

struct ResourcesInfo
{
	struct CBuffer
	{
		std::vector<float> defaultData;
		CBufferType type{};
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

enum class FastParam
{
	MaterialColor,
	TexIdDiffuse,
	COUNT
};

struct FrameGpuParameters
{
	DirectX::XMFLOAT4X4 shadowMapViewProjectionTransposed;
	DirectX::XMFLOAT3 sunDirection;
	DirectX::XMFLOAT2 inverseViewportSize;
	float time;
	float timeDelta;
};
