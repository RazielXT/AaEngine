#pragma once

#include "ShaderConstants.h"

struct ShaderTextureView;

constexpr const UINT BindlessTextureIndex = -1;

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

struct FrameGpuParameters
{
	XMFLOAT3 sunDirection;
	XMFLOAT2 inverseViewportSize;
	float time;
	XMFLOAT4X4 shadowMapViewProjectionTransposed;
};
