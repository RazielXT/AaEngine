#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

SamplerState LinearBorderSampler : register(s0);
SamplerState PointSampler : register(s1);

Texture2D colorTex : register(t0);
Texture2D<float> depthMap : register(t1);
Texture2D<float4> normalMap : register(t2);

// Relaxed bilateral parameters compared to accumulation (DepthSigma=0.1, NormalPower=8)
static const float DepthSigma = 0.3f;
static const float NormalPower = 4.0f;
static const float StepScale = 2.0f;

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
	float refDepth = depthMap.Sample(PointSampler, input.TexCoord).r;
	float3 refNormal = normalMap.Sample(PointSampler, input.TexCoord).rgb;

	float4 weightedSum = 0;
	float totalWeight = 0;

	[unroll]
	for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; i++)
	{
		float2 sampleUV = input.TexCoord + step * i;
		float4 color = colorTex.Sample(LinearBorderSampler, sampleUV);

		float sampleDepth = depthMap.Sample(PointSampler, sampleUV).r;
		float3 sampleNormal = normalMap.Sample(PointSampler, sampleUV).rgb;

		float w = BilateralWeight(refDepth, refNormal, sampleDepth, sampleNormal, spatialKernel[i + KERNEL_RADIUS]);
		weightedSum += color * w;
		totalWeight += w;
	}

	return weightedSum / max(totalWeight, 1e-5f);
}
