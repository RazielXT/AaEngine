#include "../postprocess/PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D sceneTexture : register(t0);
Texture2D waterTexture : register(t1);
Texture2D reflectionsTexture : register(t2);
Texture2D waterNormal : register(t3);
Texture2D causticsTexture : register(t4);

SamplerState LinearSampler : register(s0);
SamplerState LinearBorderSampler : register(s1);

float4 PSWaterApply(VS_OUTPUT input) : SV_TARGET
{
	float4 water = waterTexture.Load(int3(input.Position.xy,0));
	float4 normal = waterNormal.Load(int3(input.Position.xy,0));

	float2 refraction = normal.xx * 0.3f;
	float4 waterRefr = waterNormal.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0);
	refraction *= saturate(waterRefr.y * 10);

	float4 underwater = sceneTexture.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0);
	underwater.rgb += causticsTexture.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0).rrr;

	float2 reflOffset = normal.xy * 0.5f;
	reflOffset.y = 0;
	float4 reflections = reflectionsTexture.SampleLevel(LinearSampler, input.TexCoord + reflOffset, 0);

	water.rgb = lerp(water.rgb, reflections.rgb, reflections.a);
	underwater.rgb = lerp(underwater.rgb, water.rgb, water.a);

	//float3 fogColor = float3(0.6,0.6,0.7);
	//albedo.rgb = lerp(albedo.rgb, fogColor, min(0.7, saturate(camDistance / 15000)));

	return underwater;
}
