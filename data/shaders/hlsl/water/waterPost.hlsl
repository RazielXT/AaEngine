#include "../postprocess/PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D sceneTexture : register(t0);
Texture2D waterTexture : register(t1);
Texture2D reflectionsTexture : register(t2);
Texture2D waterNormal : register(t3);
Texture2D causticsTexture : register(t4);

SamplerState LinearWrapSampler : register(s0);

float4 PSWaterApply(VS_OUTPUT input) : SV_TARGET
{
	float4 water = waterTexture.Load(int3(input.Position.xy,0));
	float4 normal = waterNormal.Load(int3(input.Position.xy,0));

	float4 color = sceneTexture.SampleLevel(LinearWrapSampler, saturate(input.TexCoord + normal.xz * 0.3f), 0);
	color.rgb += causticsTexture.SampleLevel(LinearWrapSampler, saturate(input.TexCoord + normal.xz * 0.3f), 0).rrr;
	//float4 color = sceneTexture.Load(int3(input.Position.xy + normal.xy*30,0));

	float2 reflOffset = normal.xz * 0.5f;
	float4 reflections = reflectionsTexture.SampleLevel(LinearWrapSampler, input.TexCoord + reflOffset, 0);

	water.rgb = lerp(water.rgb, reflections.rgb, reflections.a);
	color.rgb = lerp(color.rgb, water.rgb, water.a);

	//float3 fogColor = float3(0.6,0.6,0.7);
	//albedo.rgb = lerp(albedo.rgb, fogColor, min(0.7, saturate(camDistance / 15000)));

	return color;
}
