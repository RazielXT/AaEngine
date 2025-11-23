#include "../postprocess/PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D sceneTexture : register(t0);
Texture2D waterTexture : register(t1);
Texture2D reflectionsTexture : register(t2);
Texture2D waterNormal : register(t3);

SamplerState LinearWrapSampler : register(s0);

float4 PSWaterApply(VS_OUTPUT input) : SV_TARGET
{
	float4 water = waterTexture.Load(int3(input.Position.xy,0));
	float4 normal = waterNormal.Load(int3(input.Position.xy,0));

	float4 color = sceneTexture.SampleLevel(LinearWrapSampler, saturate(input.TexCoord + normal.xz * 0.3f), 0);
	//float4 color = sceneTexture.Load(int3(input.Position.xy + normal.xy*30,0));
	float4 reflections = reflectionsTexture.SampleLevel(LinearWrapSampler, input.TexCoord, 0);

	water.rgb = lerp(water.rgb, reflections.rgb, reflections.a);
	color.rgb = lerp(color.rgb, water.rgb, water.a);

	//float3 fogColor = float3(0.6,0.6,0.7);
	//albedo.rgb = lerp(albedo.rgb, fogColor, min(0.7, saturate(camDistance / 15000)));

	return color;
}

float4 PSWaterBlur(VS_OUTPUT input) : SV_TARGET
{
    float3 centerColor = sceneTexture.Sample(LinearWrapSampler, input.TexCoord).rgb;
    float3 centerNormal = waterTexture.Sample(LinearWrapSampler, input.TexCoord).xyz;

    float4 sum = 0;
    float totalWeight = 0;
	const float sigma = 1;

    // 3x3 kernel example
    [unroll]
    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            float2 offset = input.TexCoord + float2(x, y) * ViewportSizeInverse * 2;
            float4 sampleColor = sceneTexture.Sample(LinearWrapSampler, offset);
            float3 sampleNormal = waterTexture.Sample(LinearWrapSampler, offset).xyz;

            // Weight by normal similarity
            float normalWeight = max(0.0, pow(dot(centerNormal, sampleNormal), 1));

            sum += sampleColor * normalWeight;
            totalWeight += normalWeight;
        }
    }

    return (sum / max(totalWeight, 1e-5));
}
