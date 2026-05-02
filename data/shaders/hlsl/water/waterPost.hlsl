#include "hlsl/postprocess/PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D sceneTexture : register(t0);
Texture2D waterTexture : register(t1);
Texture2D reflectionsTexture : register(t2);
Texture2D waterNormal : register(t3);
Texture2D causticsTexture : register(t4);

SamplerState LinearSampler : register(s0);
SamplerState LinearBorderSampler : register(s1);

float2 MirrorUV(float2 v)
{
	float2 a = abs(v); // handles negative side
	return 1.0 - abs(a - 1.0); // mirrors around 1
}

float4 PSWaterApply(VS_OUTPUT input) : SV_TARGET
{
	float4 normal = waterNormal.Load(int3(input.Position.xy,0));

	float2 refraction = normal.xx * 0.3f;
	float4 waterRefr = waterNormal.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0);
	refraction *= saturate(waterRefr.y * 10);

	float4 sceneColor = sceneTexture.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0);
	sceneColor.rgb += causticsTexture.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0).rrr;

	float reflOffset = normal.x * 0.15f;
	float2 reflectionUv = MirrorUV(input.TexCoord + float2(reflOffset, 0));

	float4 reflections = reflectionsTexture.SampleLevel(LinearSampler, reflectionUv, 0);
	float4 water = waterTexture.Load(int3(input.Position.xy,0));

	water.rgb = lerp(water.rgb, reflections.rgb, reflections.a);
	sceneColor.rgb = lerp(sceneColor.rgb, water.rgb, water.a);

	return sceneColor;
}
