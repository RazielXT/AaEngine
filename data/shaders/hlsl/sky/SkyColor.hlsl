#include "hlsl/sky/SkyParams.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/Srgb.hlsl"

float3 getSkyColor(float3 dir, SkyParams Sky, SamplerState sampler)
{
	float sunZenithDot = -Sky.SunDirection.y;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture2D(Sky.TexIdSunZenith).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb;

	float3 viewZenithColor = GetTexture2D(Sky.TexIdViewZenith).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb;
	float vzMask = pow(saturate(1.0 - dir.y), 4);

	return SrgbToLinear(sunZenithColor + viewZenithColor * vzMask);
}
