#include "PostProcessCommon.hlsl"
#include "../ShadowsPssm.hlsl"

float4x4 InvProjectionMatrix;
float4x4 InvViewMatrix;
float ResId;
float3 CameraPosition;

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

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float3 albedo = colorMap.Load(int3(input.Position.xy, 0)).rgb;
	float3 worldNormal = normalMap.Load(int3(input.Position.xy, 0)).rgb;

	if (all(worldNormal == float3(0,0,0))) return float4(albedo,0);

	float dotLighting = saturate(dot(-Sun.Direction, worldNormal));

	float depth = depthMap.Load(int3(input.Position.xy, 0)).r;
	float4 worldPosition = float4(ReconstructWorldPosition(input.TexCoord, depth, InvProjectionMatrix, InvViewMatrix), 1);
	float camDistance = length(CameraPosition - worldPosition.xyz);

	float directShadow = getPssmShadowLow(worldPosition, camDistance, dotLighting, ShadowSampler, Sun);

	float3 vctLighting = vctMap.Load(int3(input.Position.xy, 0)).rgb;
	float3 lighting = dotLighting * Sun.Color * directShadow + vctLighting;
	lighting *= lerp(ssaoMap.Sample(LinearSampler, input.TexCoord).r, 1, (lighting.r + lighting.g + lighting.b) / 3);
	
	float4 finalColor = float4(albedo * lighting, 1);

	const float3 fogColor = float3(0.6,0.6,0.7);
	finalColor.rgb = lerp(finalColor.rgb, fogColor, min(0.85, saturate((camDistance - 1000) / 14000)));

	return finalColor;
}
