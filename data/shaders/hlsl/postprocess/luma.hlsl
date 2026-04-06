#include "PostProcessCommon.hlsl"

Texture2D colorMap : register(t0);

SamplerState LinearSampler : register(s0);

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float4 original = colorMap.Sample(LinearSampler, input.TexCoord);

	float luma = dot(original.rgb, float3(0.299, 0.587, 0.114));
	original.w = luma;

	return original;
}
