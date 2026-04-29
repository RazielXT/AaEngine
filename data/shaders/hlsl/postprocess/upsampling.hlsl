#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D<float4> LowColor : register(t0);
Texture2D<float> HighDepth : register(t1);
Texture2D<float> LowDepth : register(t2);
Texture2D<float4> HighNormal : register(t3);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

float4 BilateralUpsample(
	float HiDepth,
	float3 HiNormal,
	float4 LowDepths,
	float3 LowNormals[4],
	float4 LowColors[4])
{
	// Depth weights
	float sigma = 0.1; // tune this
	float4 depthDiff = abs(HiDepth - LowDepths) / max(HiDepth, 1e-3);
	float4 depthWeights = exp(-depthDiff / sigma);

	// Normal weights: use dot product similarity
	float4 normalWeights;
	normalWeights.x = pow(saturate(dot(HiNormal, LowNormals[0])), 8);
	normalWeights.y = pow(saturate(dot(HiNormal, LowNormals[1])), 8);
	normalWeights.z = pow(saturate(dot(HiNormal, LowNormals[2])), 8);
	normalWeights.w = pow(saturate(dot(HiNormal, LowNormals[3])), 8);

	// Combine depth + normal
	float4 weights = depthWeights * normalWeights;

	float threshold = 0.02; // tune
	for (int i = 0; i < 4; i++)
		if (abs(HiDepth - LowDepths[i]) > threshold)
		{
			weights[i] = 0;
		}

	const float NoiseFilterStrength = 0.01;
	float TotalWeight = dot(weights, 1.0) + NoiseFilterStrength;

	float4 WeightedSum = 0;
	WeightedSum += LowColors[0] * weights.x;
	WeightedSum += LowColors[1] * weights.y;
	WeightedSum += LowColors[2] * weights.z;
	WeightedSum += LowColors[3] * weights.w;

	return WeightedSum / TotalWeight;
}

float4 PS_UpsampleDepthAware(VS_OUTPUT input) : SV_Target
{
	float2 pixel = input.Position.xy; // high-res pixel coord
	float2 lowPixel = pixel * 0.5; // map to low-res pixel space
	float2 base = floor(lowPixel - 0.5) + 0.5; // snap to texel center
	float2 lowInvSize = 2 * ViewportSizeInverse;

	// Convert back to UVs
	float2 uvTL = base * lowInvSize;
	float2 uvTR = (base + float2(1,0)) * lowInvSize;
	float2 uvBL = (base + float2(0,1)) * lowInvSize;
	float2 uvBR = (base + float2(1,1)) * lowInvSize;
	float2 uv = input.TexCoord.xy;

	// Sample colors
	float4 lowColors[4];
	lowColors[0] = LowColor.Sample(PointSampler, uvTL);
	lowColors[1] = LowColor.Sample(PointSampler, uvTR);
	lowColors[2] = LowColor.Sample(PointSampler, uvBL);
	lowColors[3] = LowColor.Sample(PointSampler, uvBR);

	// Sample depths
	float4 LowDepths;
	LowDepths.x = LowDepth.Sample(PointSampler, uvTL);
	LowDepths.y = LowDepth.Sample(PointSampler, uvTR);
	LowDepths.z = LowDepth.Sample(PointSampler, uvBL);
	LowDepths.w = LowDepth.Sample(PointSampler, uvBR);

	float refDepth = HighDepth.Sample(PointSampler, uv);

	float3 lowNormals[4];
	lowNormals[0] = normalize(HighNormal.Sample(PointSampler, uvTL).xyz);
	lowNormals[1] = normalize(HighNormal.Sample(PointSampler, uvTR).xyz);
	lowNormals[2] = normalize(HighNormal.Sample(PointSampler, uvBL).xyz);
	lowNormals[3] = normalize(HighNormal.Sample(PointSampler, uvBR).xyz);

	float3 refNormal = normalize(HighNormal.Sample(PointSampler, uv).xyz);

	return BilateralUpsample(refDepth, refNormal, LowDepths, lowNormals, lowColors);
}