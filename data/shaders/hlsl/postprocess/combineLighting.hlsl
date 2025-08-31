#include "PostProcessCommon.hlsl"

Texture2D colorMap : register(t0);
SamplerState LinearSampler : register(s0);
Texture2D<float> distanceMap : register(t1);

float4 PSCombineLighting(VS_OUTPUT input) : SV_TARGET
{
	float4 original = colorMap.Sample(LinearSampler, input.TexCoord);

	float3 fogColor = float3(0.6,0.6,0.7);
	float camDistance = distanceMap.Load(int3(input.Position.xy, 0)).r;
	original.rgb = lerp(original.rgb, fogColor, min(0.85, saturate((camDistance - 1000) / 14000)));

    return original;
}
