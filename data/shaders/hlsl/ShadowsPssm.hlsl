#include "ShadowsCommon.hlsl"

float getCascadeShadow(float4 wp, uint ShadowIndex, float ShadowBias, SamplerState sampler, SunParams params)
{
	Texture2D<float> shadowmap = ResourceDescriptorHeap[params.TexIdShadowOffset + ShadowIndex];
    float4 sunLookPos = mul(wp, params.ShadowMatrix[ShadowIndex]);
    sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;
	sunLookPos.z -= ShadowBias;

	return CalcShadowTermSoftPCF(shadowmap, sampler, sunLookPos.z, sunLookPos.xy, 5, params.ShadowMapSize, params.ShadowMapSizeInv);
	
	//return CalculatePCFPercentLit(shadowmap, sampler, sunLookPos.z, sunLookPos.xy, params.ShadowMapSizeInv);
}

float getPssmShadowSimple(float4 wp, float cameraDistance, float dotView, SamplerState sampler, SunParams params)
{
	float biasSlopeAdjust = 1;

	if (dotView < 0.5)
		biasSlopeAdjust += (0.5 - dotView) * 5;

	uint ShadowIndex = 0;
	float ShadowBias[3] = { 0.001, 0.005, 0.010 };
	float lerpPoint = cameraDistance / params.ShadowCascadeDistance0;

	if (params.ShadowCascadeDistance0 < cameraDistance)
	{
		ShadowIndex = 1;
		lerpPoint = (cameraDistance - params.ShadowCascadeDistance0) / (params.ShadowCascadeDistance1 - params.ShadowCascadeDistance0);
	}
	if (params.ShadowCascadeDistance1 < cameraDistance)
	{
		ShadowIndex = 2;
		lerpPoint = (cameraDistance - params.ShadowCascadeDistance1) / (params.ShadowCascadeDistance2 - params.ShadowCascadeDistance1);
	}

	return getCascadeShadow(wp, ShadowIndex, ShadowBias[ShadowIndex] * biasSlopeAdjust, sampler, params);
}

float getPssmShadow(float4 wp, float cameraDistance, float dotView, SamplerState sampler, SunParams params)
{
	float biasSlopeAdjust = 1;

	if (dotView < 0.5)
		biasSlopeAdjust += (0.5 - dotView) * 5;

	uint ShadowIndex = 0;
	float ShadowBias[3] = { 0.001, 0.005, 0.005 };
	float lerpPoint = cameraDistance / params.ShadowCascadeDistance0;

	if (params.ShadowCascadeDistance0 < cameraDistance)
	{
		ShadowIndex = 1;
		lerpPoint = (cameraDistance - params.ShadowCascadeDistance0) / (params.ShadowCascadeDistance1 - params.ShadowCascadeDistance0);
	}
	if (params.ShadowCascadeDistance1 < cameraDistance)
	{
		ShadowIndex = 2;
		lerpPoint = (cameraDistance - params.ShadowCascadeDistance1) / (params.ShadowCascadeDistance2 - params.ShadowCascadeDistance1);
	}

	float shadow = getCascadeShadow(wp, ShadowIndex, ShadowBias[ShadowIndex] * biasSlopeAdjust, sampler, params);
	
	if (ShadowIndex != 2)
		shadow = lerp(shadow, getCascadeShadow(wp, ShadowIndex + 1, ShadowBias[ShadowIndex + 1] * biasSlopeAdjust, sampler, params), lerpPoint);
		
	return shadow;
}