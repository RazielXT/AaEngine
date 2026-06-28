#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

SamplerState LinearBorderSampler : register(s0);
SamplerState PointSampler : register(s1);

Texture2D colorTex : register(t0);
Texture2D<float> depthMap : register(t1);
Texture2D<float4> normalMap : register(t2);

// Relaxed bilateral parameters compared to accumulation (DepthSigma=0.1, NormalPower=8)
static const float DepthSigma = 0.05f;
static const float NormalPower = 2.0f;
static const float StepScale = 1.0f;

#define KERNEL_RADIUS 4
static const float spatialKernel[9] = { 0.028, 0.066, 0.124, 0.179, 0.206, 0.179, 0.124, 0.066, 0.028 };

float BilateralWeight(float refDepth, float3 refNormal, float sampleDepth, float3 sampleNormal, float spatialWeight)
{
	float depthDiff = abs(refDepth - sampleDepth) / max(refDepth, 1e-5f);
	float depthWeight = exp(-depthDiff / DepthSigma);

	float normalWeight = pow(saturate(dot(refNormal, sampleNormal)), NormalPower);

	return spatialWeight * depthWeight * normalWeight;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
#ifdef BLUR_VERTICAL
	float2 dir = float2(0, 1);
#else
	float2 dir = float2(1, 0);
#endif

	float2 step = dir * ViewportSizeInverse * StepScale;

	// The GI color is half-res (texel p == top-left full-res texel 2*p, matching the trace and
	// linearDepthDownsample2). The full-res depth/normal guide must sample that same texel 2*p,
	// not the half-res pixel center (which point-samples full-res texel 2*p+1).
	float2 depthUvOffset = -0.25f * ViewportSizeInverse;

	float refDepth = depthMap.Sample(PointSampler, input.TexCoord + depthUvOffset).r;
	float3 refNormal = normalMap.Sample(PointSampler, input.TexCoord + depthUvOffset).rgb;

	float4 weightedSum = 0;
	float totalWeight = 0;

	[unroll]
	for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; i++)
	{
		float2 sampleUV = input.TexCoord + step * i;
		float4 color = colorTex.Sample(LinearBorderSampler, sampleUV);

		float sampleDepth = depthMap.Sample(PointSampler, sampleUV + depthUvOffset).r;
		float3 sampleNormal = normalMap.Sample(PointSampler, sampleUV + depthUvOffset).rgb;

		float w = BilateralWeight(refDepth, refNormal, sampleDepth, sampleNormal, spatialKernel[i + KERNEL_RADIUS]);
		weightedSum += color * w;
		totalWeight += w;
	}

	return weightedSum / max(totalWeight, 1e-5f);
}
