#include "hlsl/postprocess/PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D normalMap : register(t0);

SamplerState LinearSampler : register(s0);

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float2 normal = normalMap.Sample(LinearSampler, input.TexCoord).xy;
	
	float color = saturate(abs(normal.x)- 0.15f) * 0.15f;
	
	return float4(color.xxx, 0);
}
