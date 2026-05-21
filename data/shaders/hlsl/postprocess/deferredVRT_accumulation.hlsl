#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

SamplerState LinearSampler : register(s0);

float4 Gaussian3x3(Texture2D InputTex, float2 uv, float2 t)
{
	float4 c00 = InputTex.Sample(LinearSampler, uv + float2(-t.x, -t.y));
	float4 c10 = InputTex.Sample(LinearSampler, uv + float2( 0.0f, -t.y));
	float4 c20 = InputTex.Sample(LinearSampler, uv + float2( t.x, -t.y));

	float4 c01 = InputTex.Sample(LinearSampler, uv + float2(-t.x,  0.0f));
	float4 c11 = InputTex.Sample(LinearSampler, uv);
	float4 c21 = InputTex.Sample(LinearSampler, uv + float2( t.x,  0.0f));

	float4 c02 = InputTex.Sample(LinearSampler, uv + float2(-t.x,  t.y));
	float4 c12 = InputTex.Sample(LinearSampler, uv + float2( 0.0f,  t.y));
	float4 c22 = InputTex.Sample(LinearSampler, uv + float2( t.x,  t.y));

	float4 result =
		  (c00 + c20 + c02 + c22) * 1.0f   // corners
		+ (c10 + c01 + c21 + c12) * 2.0f   // edges
		+  c11 * 4.0f;                     // center

	return result * (1.0f / 16.0f);
}

Texture2D currentRays : register(t0);
Texture2D accumulatedRays : register(t1);


float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float4 current = currentRays.Load(int3(input.Position.xy, 0));
	float4 accumulated = Gaussian3x3(accumulatedRays, input.TexCoord, ViewportSizeInverse);
	return lerp(current, accumulated, 0.95f);
}