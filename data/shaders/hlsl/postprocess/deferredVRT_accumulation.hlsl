#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

SamplerState LinearBorderSampler : register(s0);

float4 Gaussian3x3(Texture2D InputTex, float2 uv, float2 t)
{
	float4 c00 = InputTex.Sample(LinearBorderSampler, uv + float2(-t.x, -t.y));
	float4 c10 = InputTex.Sample(LinearBorderSampler, uv + float2( 0.0f, -t.y));
	float4 c20 = InputTex.Sample(LinearBorderSampler, uv + float2( t.x, -t.y));

	float4 c01 = InputTex.Sample(LinearBorderSampler, uv + float2(-t.x,  0.0f));
	float4 c11 = InputTex.Sample(LinearBorderSampler, uv);
	float4 c21 = InputTex.Sample(LinearBorderSampler, uv + float2( t.x,  0.0f));

	float4 c02 = InputTex.Sample(LinearBorderSampler, uv + float2(-t.x,  t.y));
	float4 c12 = InputTex.Sample(LinearBorderSampler, uv + float2( 0.0f,  t.y));
	float4 c22 = InputTex.Sample(LinearBorderSampler, uv + float2( t.x,  t.y));

	float4 result =
		  (c00 + c20 + c02 + c22) * 1.0f   // corners
		+ (c10 + c01 + c21 + c12) * 2.0f   // edges
		+  c11 * 4.0f;                     // center

	return result * (1.0f / 16.0f);
}

Texture2D currentRays : register(t0);
Texture2D accumulatedRays : register(t1);
Texture2D<float2> motionVectors : register(t2);


float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float2 motionPixels = motionVectors.Load(int3(input.Position.xy * 2, 0));
	float2 motionUV = motionPixels * ViewportSizeInverse * 0.5f;

	float4 current = currentRays.Load(int3(input.Position.xy, 0));
	float4 accumulated = Gaussian3x3(accumulatedRays, input.TexCoord + motionUV, ViewportSizeInverse);
	return lerp(current, accumulated, 0.9f);
}