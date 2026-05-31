#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

SamplerState PointSampler : register(s0);

Texture2D<float> agePrev : register(t0);
Texture2D<float2> motionVectors : register(t1);

float PSMain(VS_OUTPUT input) : SV_TARGET
{
	// Motion vectors are full-res, this pass is half-res
	float2 motionPixels = motionVectors.Load(int3(input.Position.xy * 2, 0));
	float2 motionUV = motionPixels * ViewportSizeInverse * 0.5f;
	float2 prevUV = input.TexCoord + motionUV;

	// Off-screen reprojection means fresh pixel
	if (any(prevUV < 0) || any(prevUV > 1))
		return 0;

	float prevAge = agePrev.Sample(PointSampler, prevUV);
	return min(prevAge + 1.0f, 255.0f);
}
