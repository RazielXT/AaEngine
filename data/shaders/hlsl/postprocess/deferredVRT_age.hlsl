#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

SamplerState PointSampler : register(s0);

Texture2D<float> agePrev : register(t0);
Texture2D<float2> motionVectors : register(t1);
Texture2D<float> depthMap : register(t2);
Texture2D<float4> normalMap : register(t3);
Texture2D<float> depthMapPrev : register(t4);
Texture2D<float4> normalMapPrev : register(t5);

static const float DepthSigma = 0.3f;
static const float NormalPower = 4.0f;
static const float HistoryAcceptance = 0.1f;

float PSMain(VS_OUTPUT input) : SV_TARGET
{
	// Motion vectors are full-res, this pass is half-res
	float2 motionPixels = motionVectors.Load(int3(input.Position.xy * 2, 0));
	float2 motionUV = motionPixels * ViewportSizeInverse * 0.5f;
	float2 prevUV = input.TexCoord + motionUV;

	// Off-screen reprojection means fresh pixel
	if (any(prevUV < 0) || any(prevUV > 1))
		return 0;

	// The GI is half-res (texel p == top-left full-res texel 2*p, matching the trace and
	// linearDepthDownsample2). Sample the full-res depth/normal guide at that same texel 2*p
	// instead of the half-res pixel center (which point-samples full-res texel 2*p+1).
	float2 depthUvOffset = -0.25f * ViewportSizeInverse;

	float currentDepth = depthMap.Sample(PointSampler, input.TexCoord + depthUvOffset).r;
	float prevDepth = depthMapPrev.Sample(PointSampler, prevUV + depthUvOffset).r;
	float depthDiff = abs(currentDepth - prevDepth) / max(currentDepth, 1e-5f);
	float depthWeight = exp(-depthDiff / DepthSigma);

	float3 currentNormal = normalMap.Sample(PointSampler, input.TexCoord + depthUvOffset).rgb;
	float3 prevNormal = normalMapPrev.Sample(PointSampler, prevUV + depthUvOffset).rgb;
	float normalWeight = pow(saturate(dot(currentNormal, prevNormal)), NormalPower);

	float historyWeight = depthWeight * normalWeight;
	if (historyWeight < HistoryAcceptance)
		return 0;

	float prevAge = agePrev.Sample(PointSampler, prevUV);
	return min(prevAge + 1.0f, 255.0f);
}
