#include "hlsl/sky/SkyParams.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/Srgb.hlsl"

float3 getSkyColor(float3 dir, SkyParams Sky, SamplerState sampler)
{
	float sunZenithDot = -Sky.SunDirection.y;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture2D(Sky.TexIdSunZenith).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb * 0.5f;

	float3 viewZenithColor = GetTexture2D(Sky.TexIdViewZenith).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb * 0.5f;
	float vzMask = pow(saturate(1.0 - dir.y), 4);

	return SrgbToLinear(sunZenithColor + viewZenithColor * vzMask);
}

float3 getSkyFullColor(float3 dir, SkyParams Sky, SamplerState sampler)
{
	float sunDot = dot(-dir, Sky.SunDirection);
	float sunZenithDot = -Sky.SunDirection.y;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture2D(Sky.TexIdSunZenith).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb * 0.5f;

	float3 viewZenithColor = GetTexture2D(Sky.TexIdViewZenith).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb * 0.5f;
	float vzMask = pow(saturate(1.0 - dir.y), 4);

	float3 sunViewColor = GetTexture2D(Sky.TexIdSunView).Sample(sampler, float2(sunZenithDot01, 0.5)).rgb;
	float svMask = pow(saturate(sunDot), 4);

	return SrgbToLinear(sunZenithColor + viewZenithColor * vzMask + sunViewColor * svMask);
}
