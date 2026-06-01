#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

SamplerState LinearBorderSampler : register(s0);
SamplerState PointSampler : register(s1);

Texture2D currentRays : register(t0);
Texture2D accumulatedRays : register(t1);
Texture2D<float2> motionVectors : register(t2);
Texture2D<float> depthMap : register(t3);
Texture2D<float4> normalMap : register(t4);
Texture2D<float> ageTex : register(t5);
Texture2D<float> depthMapPrev : register(t6);
Texture2D<float4> normalMapPrev : register(t7);
Texture2D blurredCurrentRays : register(t8);

static const float DepthSigma = 0.3f;
static const float NormalPower = 4.0f;
static const float HistoryAcceptance = 0.1f;

float BilateralWeight(float refDepth, float3 refNormal, float sampleDepth, float3 sampleNormal, float spatialWeight)
{
	float depthDiff = abs(refDepth - sampleDepth) / max(refDepth, 1e-5f);
	float depthWeight = exp(-depthDiff / DepthSigma);

	float normalWeight = pow(saturate(dot(refNormal, sampleNormal)), NormalPower);

	return spatialWeight * depthWeight * normalWeight;
}

float4 BilateralGaussian3x3(Texture2D InputTex, float2 uv, float2 t, float refDepth, float3 refNormal)
{
	static const float spatialKernel[9] = { 1, 2, 1, 2, 4, 2, 1, 2, 1 };
	static const float2 offsets[9] = {
		float2(-1, -1), float2(0, -1), float2(1, -1),
		float2(-1,  0), float2(0,  0), float2(1,  0),
		float2(-1,  1), float2(0,  1), float2(1,  1)
	};

	float4 weightedSum = 0;
	float totalWeight = 0;

	[unroll]
	for (int i = 0; i < 9; i++)
	{
		float2 sampleUV = uv + offsets[i] * t;
		float4 color = InputTex.Sample(LinearBorderSampler, sampleUV);

		float sampleDepth = depthMap.Sample(PointSampler, sampleUV).r;
		float3 sampleNormal = normalMap.Sample(PointSampler, sampleUV).rgb;

		float w = BilateralWeight(refDepth, refNormal, sampleDepth, sampleNormal, spatialKernel[i]);
		weightedSum += color * w;
		totalWeight += w;
	}

	return weightedSum / max(totalWeight, 1e-5f);
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float2 motionPixels = motionVectors.Load(int3(input.Position.xy * 2, 0));
	float2 motionUV = motionPixels * ViewportSizeInverse * 0.5f;

	float refDepth = depthMap.Sample(PointSampler, input.TexCoord).r;
	float3 refNormal = normalMap.Sample(PointSampler, input.TexCoord).rgb;

	float4 current = currentRays.Load(int3(input.Position.xy, 0));
	float4 blurredCurrent = blurredCurrentRays.Load(int3(input.Position.xy, 0));

	float2 reprojectedUV = input.TexCoord + motionUV;

	float accumulationFactor = 0.98f;

	const float MaxAge = 8.0f;
	float age = ageTex.Load(int3(input.Position.xy, 0));
	float freshness = saturate(age / MaxAge);
	accumulationFactor *= freshness;

	// Reject only clearly invalid history; keep full weight for valid reprojection.
	if (any(reprojectedUV < 0) || any(reprojectedUV > 1))
		return blurredCurrent;
	float prevDepth = depthMapPrev.Sample(PointSampler, reprojectedUV).r;
	float3 prevNormal = normalMapPrev.Sample(PointSampler, reprojectedUV).rgb;
	float depthDiff = abs(refDepth - prevDepth) / max(refDepth, 1e-5f);
	float historyValidity = exp(-depthDiff / DepthSigma) * pow(saturate(dot(refNormal, prevNormal)), NormalPower);
	if (historyValidity < HistoryAcceptance)
		return blurredCurrent;

	return lerp(current, accumulated, accumulationFactor);
}