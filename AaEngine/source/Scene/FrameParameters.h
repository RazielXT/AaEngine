#pragma once

#include <DirectXMath.h>
#include "Utils/MathUtils.h"

using namespace DirectX;

struct SkyParameters
{
	XMFLOAT4X4 ShadowMatrix[4];

	Vector3 SunDirection;
	UINT TexIdShadowMap0;

	float ShadowCascadeDistance[4];

	Vector3 SunColor;
	float ShadowMapSize;

	float ShadowMapSizeInv;
	UINT TexIdSunZenith;
	UINT TexIdViewZenith;
	UINT TexIdSunView;

	Vector3 MoonDirection;
	float CloudsAmount{};
	float CloudsDensity = 0.5f;
	float CloudsSpeed = 0.003f;
};

struct FrameParameters
{
	SkyParameters sky;

	float time{};
	float timeDelta{};
	UINT frameIndex{};
	UINT frameCounter{};
};
