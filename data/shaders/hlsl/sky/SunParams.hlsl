#pragma once

struct SunParams
{
	float4x4 ShadowMatrix[4];

	float3 Direction;
	uint TexIdShadowMap0;

	float ShadowCascadeDistance0;
	float ShadowCascadeDistance1;
	float ShadowCascadeDistance2;
	float ShadowCascadeDistance3;

	float3 Color;
	float ShadowMapSize;

	float ShadowMapSizeInv;
	uint TexIdSunZenith;
	uint TexIdViewZenith;
	uint TexIdSunView;

	float CloudsAmount;
	float CloudsDensity;
	float CloudsSpeed;
};
