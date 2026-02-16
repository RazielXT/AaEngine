#include "PostProcessCommon.hlsl"
#include "WorldReconstruction.hlsl"
#include "hlsl/ShadowsPssm.hlsl"

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
uint ResId;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

Texture2D colorMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D<float> depthMap : register(t2);
Texture2D<float> ssaoMap : register(t3);
Texture2D vctMap : register(t4);

SamplerState ShadowSampler : register(s0);
SamplerState LinearSampler : register(s1);

float3 SrgbToLinear(float3 srgbColor)
{
	return pow(srgbColor, 2.233333333);
}

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

float3 getFogColor(float3 dir)
{
	float sunZenithDot = -Sun.Direction.y;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture(Sun.TexIdSunZenith).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb;

	float3 viewZenithColor = GetTexture(Sun.TexIdViewZenith).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb;
	float vzMask = pow(saturate(1.0 - dir.y), 4);

	return SrgbToLinear(sunZenithColor + viewZenithColor * vzMask);
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float4 albedo = colorMap.Load(int3(input.Position.xy, 0));
	float emmisive = albedo.w;
	float3 worldNormal = normalMap.Load(int3(input.Position.xy, 0)).rgb;

	if (all(worldNormal == float3(0,0,0))) return float4(albedo.rgb,0);

	float dotLighting = saturate(dot(-Sun.Direction, worldNormal));
	//dotLighting *= dotLighting;

	float depth = depthMap.Load(int3(input.Position.xy, 0)).r;
	float4 worldPosition = float4(ReconstructWorldPosition(input.TexCoord, depth, InvViewProjectionMatrix), 1);
	float camDistance = length(CameraPosition - worldPosition.xyz);

	float directShadow = getPssmShadow(worldPosition, camDistance, dotLighting, ShadowSampler, Sun);

	float3 vctLighting = vctMap.Load(int3(input.Position.xy, 0)).rgb * 0.5;// + worldNormal.y * float3(0.1,0.2,0.3) * 0.2;// * saturate(dot(Sun.Direction, worldNormal) + 0.5);
	float3 lighting = dotLighting * Sun.Color * directShadow + vctLighting + emmisive * 10;
	lighting *= lerp(ssaoMap.Sample(LinearSampler, input.TexCoord).r, 1, saturate((lighting.r + lighting.g + lighting.b) / 3));

	float4 finalColor = float4(albedo.rgb * lighting, 1);

	float3 fogPosCheck = worldPosition.xyz;
	//fogPosCheck.y = 0;
	float3 fogAtmDir = normalize(CameraPosition - fogPosCheck);
	fogAtmDir.y = saturate(fogAtmDir.y);
	const float3 fogColor = getFogColor(fogAtmDir) / 2;

	float fogDensity = 0.000000001;
	float fogFactor = 1.0 - exp(-camDistance * camDistance * fogDensity);
	finalColor.rgb = lerp(finalColor.rgb, fogColor, fogFactor);


/*
	float fogDensity = 0.000000001;
	float fogHeight = 0;
	float heightFalloff = 0.0035f;

	float heightDiff = worldPosition.y - fogHeight;
	float fogFactor = exp(-heightDiff * heightFalloff);
	fogFactor = 1.0 - exp(-camDistance * fogDensity * fogFactor);
	fogFactor = max(fogFactor, 1.0 - exp(-camDistance * camDistance * fogDensity));
	finalColor.rgb = lerp(finalColor.rgb, fogColor, fogFactor);
*/
	return finalColor;
}
