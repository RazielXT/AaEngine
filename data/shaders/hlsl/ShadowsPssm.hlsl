#include "ShadowsCommon.hlsl"
#include "hlsl/utils/blueNoise.hlsl"

float3 getCascadeShadow(float4 wp, uint ShadowIndex, float ShadowBias, SamplerState sampler, SunParams params)
{
	Texture2D<float> shadowmap = ResourceDescriptorHeap[params.TexIdShadowMap0 + ShadowIndex];
    float4 sunLookPos = mul(wp, params.ShadowMatrix[ShadowIndex]);
    sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;

	sunLookPos.z -= ShadowBias;

	float shadow = CalcShadowTermSoftPCF(shadowmap, sampler, sunLookPos.z, sunLookPos.xy, 5, params.ShadowMapSize, params.ShadowMapSizeInv);
	return float3(shadow, sunLookPos.xy);
}

float shadowBorderBlend(float2 uv, float edgeThreshold)
{
	// Calculate the distance from the UV coordinates to the nearest edge
	float left = uv.x;
	float right = 1.0 - uv.x;
	float top = uv.y;
	float bottom = 1.0 - uv.y;

	// Determine the minimum distance to an edge
	float minDist = min(min(left, right), min(top, bottom));

	// Perform linear interpolation (lerp) based on the dist / edgeThresholdance to the edge
	float alpha = saturate(minDist / edgeThreshold);

	return alpha;
}

float getPssmShadow(float4 wp, float cameraDistance, float dotView, SamplerState sampler, SunParams params)
{
	float biasSlopeAdjust = 1;

	if (dotView < 0.5)
		biasSlopeAdjust += (0.5 - dotView) * 5;

	uint ShadowIndex = 0;
	float ShadowBias[4] = { 0.00005, 0.00005, 0.0003, 0.0003 };
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
	if (params.ShadowCascadeDistance2 < cameraDistance)
	{
		ShadowIndex = 3;
		lerpPoint = (cameraDistance - params.ShadowCascadeDistance2) / (params.ShadowCascadeDistance3 - params.ShadowCascadeDistance1);
	}

	lerpPoint *= lerpPoint;

	/*float d = BlueNoise((wp.xz + wp.y) * 5);
	if (ShadowIndex < 3 && (lerpPoint - 0.5) > (d / 2))
		ShadowIndex++;*/

	float3 shadow = getCascadeShadow(wp, ShadowIndex, ShadowBias[ShadowIndex] * biasSlopeAdjust, sampler, params);
	float3 nextShadow = 1;

#ifdef CFG_SHADOW_HIGH
	if (ShadowIndex < 3)
		nextShadow = getCascadeShadow(wp, ShadowIndex + 1, ShadowBias[ShadowIndex + 1] * biasSlopeAdjust, sampler, params);
#else
	if (ShadowIndex < 3)
		lerpPoint = 0;
#endif

	if (ShadowIndex == 3)
	{
		// maximize last shadowmap coverage with falloff
		lerpPoint = min(lerpPoint * lerpPoint, 1 - shadowBorderBlend(saturate(shadow.yz), 0.25f));
		lerpPoint = pow(lerpPoint, 3);
		//lerpPoint = 0;
	}

	return lerp(shadow.x, nextShadow.x, lerpPoint);
}
