#include "hlsl/ShadowsCommon.hlsl"
#include "hlsl/utils/srgb.hlsl"

Texture2D<float4> GetTexture4f(uint index)
{
	return ResourceDescriptorHeap[index];
}

float3 getFogColor(float3 dir, SunParams Sun, SamplerState sampler)
{
	float sunZenithDot = -Sun.Direction.y;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture4f(Sun.TexIdSunZenith).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb;

	float3 viewZenithColor = GetTexture4f(Sun.TexIdViewZenith).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb;
	float vzMask = pow(saturate(1.0 - dir.y), 4);

	return SrgbToLinear(sunZenithColor + viewZenithColor * vzMask);
}
