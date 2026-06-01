#pragma once

struct SkyParams
{
	float4x4 ShadowMatrix[4];

	float3 SunDirection;
	uint TexIdShadowMap0;

	float ShadowCascadeDistance0;
	float ShadowCascadeDistance1;
	float ShadowCascadeDistance2;
	float ShadowCascadeDistance3;

	float3 SunColor;
	float ShadowMapSize;

	float ShadowMapSizeInv;
	uint TexIdSunZenith;
	uint TexIdViewZenith;
	uint TexIdSunView;

	float3 MoonDirection;
	float EclipseFactor;

	float CloudsAmount;
	float CloudsDensity;
	float CloudsSpeed;
};