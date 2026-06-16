#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

SamplerState LinearSampler : register(s0);
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

// Returns false (via totalWeight==0) when the reprojected history is geometrically
// incompatible with the current pixel, so the caller can fall back to a fresh source
// instead of producing a divide-by-epsilon black.
float4 BilateralGaussian3x3(Texture2D InputTex, float2 uv, float2 t, float refDepth, float3 refNormal, out float totalWeight)
{
	static const float spatialKernel[9] = { 1, 2, 1, 2, 4, 2, 1, 2, 1 };
	static const float2 offsets[9] = {
		float2(-1, -1), float2(0, -1), float2(1, -1),
		float2(-1,  0), float2(0,  0), float2(1,  0),
		float2(-1,  1), float2(0,  1), float2(1,  1)
	};

	float4 weightedSum = 0;
	totalWeight = 0;

	[unroll]
	for (int i = 0; i < 9; i++)
	{
		float2 sampleUV = uv + offsets[i] * t;
		float4 color = InputTex.Sample(LinearSampler, sampleUV);

		// Compare against the PREVIOUS frame at the reprojected location, which is
		// the surface that produced the history we are about to blend in.
		float sampleDepth = depthMapPrev.Sample(PointSampler, sampleUV).r;
		float3 sampleNormal = normalMapPrev.Sample(PointSampler, sampleUV).rgb;

		float w = BilateralWeight(refDepth, refNormal, sampleDepth, sampleNormal, spatialKernel[i]);
		weightedSum += color * w;
		totalWeight += w;
	}

	if (totalWeight <= 1e-5f)
		return 0;

	return weightedSum / totalWeight;
}

struct PSOutput
{
	float4 raw : SV_Target0;
	float4 accumulated : SV_Target1;
};

PSOutput PSMain(VS_OUTPUT input)
{
	float2 motionPixels = motionVectors.Load(int3(input.Position.xy * 2, 0));
	float2 motionUV = motionPixels * ViewportSizeInverse * 0.5f;

	float refDepth = depthMap.Sample(PointSampler, input.TexCoord).r;
	float3 refNormal = normalMap.Sample(PointSampler, input.TexCoord).rgb;

	float4 current = currentRays.Load(int3(input.Position.xy, 0));
	float4 blurredCurrent = blurredCurrentRays.Load(int3(input.Position.xy, 0));

	float2 reprojectedUV = input.TexCoord + motionUV;
	float bilateralWeight;
	float4 accumulated = BilateralGaussian3x3(accumulatedRays, reprojectedUV, ViewportSizeInverse, refDepth, refNormal, bilateralWeight);

	PSOutput output;
	output.raw = blurredCurrent;
	output.accumulated = blurredCurrent;

	// If the bilateral kernel rejected every tap, the reprojected history is invalid
	// for this pixel (typical on disocclusion edges when rotating the camera). Use
	// the freshly blurred current ray result instead of a 0 -> black ghost.
	if (bilateralWeight <= 1e-5f)
		return output;

	float accumulationFactor = 0.98f;

	const float MaxAge = 4.0f;
	float age = ageTex.Load(int3(input.Position.xy, 0));
	float freshness = saturate(age / MaxAge);
	freshness = 1 - pow(1-freshness, 2);

	accumulationFactor *= freshness;

	// Reject only clearly invalid history; keep full weight for valid reprojection.
	if (any(reprojectedUV < 0) || any(reprojectedUV > 1))
		return output;
	float prevDepth = depthMapPrev.Sample(PointSampler, reprojectedUV).r;
	float3 prevNormal = normalMapPrev.Sample(PointSampler, reprojectedUV).rgb;
	float depthDiff = abs(refDepth - prevDepth) / max(refDepth, 1e-5f);
	float historyValidity = exp(-depthDiff / DepthSigma) * pow(saturate(dot(refNormal, prevNormal)), NormalPower);
	if (historyValidity < HistoryAcceptance)
		return output;

	output.raw = lerp(current, accumulated, accumulationFactor);
	output.accumulated = accumulated;
	return output;
}