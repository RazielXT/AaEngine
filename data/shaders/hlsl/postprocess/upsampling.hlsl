#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D<float4> g_Color : register(t0);
Texture2D<float>  g_Depth : register(t1);
Texture2D<float>  g_LowDepth : register(t2);
Texture2D<float4> g_Normal : register(t3);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

float4 BilateralUpsample(
    float  HiDepth,
    float3 HiNormal,
    float4 LowDepths,
    float3 LowNormals[4],
    float4 LowColors[4])
{
    const float NoiseFilterStrength = 0.01;
    const float kUpsampleTolerance  = 1e-6;

    // Depth weights
    float4 depthWeights = 1.0 / (abs(HiDepth - LowDepths) + kUpsampleTolerance);

    // Normal weights: use dot product similarity
    float4 normalWeights;
    normalWeights.x = max(0.0, dot(HiNormal, LowNormals[0]));
    normalWeights.y = max(0.0, dot(HiNormal, LowNormals[1]));
    normalWeights.z = max(0.0, dot(HiNormal, LowNormals[2]));
    normalWeights.w = max(0.0, dot(HiNormal, LowNormals[3]));

    // Combine depth + normal
    float4 weights = depthWeights * normalWeights;

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
    float2 uv = input.TexCoord.xy;

    // Color UVs (half-res)
	float2 ColorSize = 2 * ViewportSizeInverse;
    float2 uvTL_Color = uv + float2(0.5,0.5) * ColorSize;
    float2 uvTR_Color = uv + float2(-0.5, 0.5) * ColorSize;
    float2 uvBL_Color = uv + float2(0.5, -0.5) * ColorSize;
    float2 uvBR_Color = uv + float2(-0.5,-0.5) * ColorSize;

    // Sample colors
	float4 lowColors[4];
    lowColors[0] = g_Color.Sample(LinearSampler, uvTL_Color);
    lowColors[1] = g_Color.Sample(LinearSampler, uvTR_Color);
    lowColors[2] = g_Color.Sample(LinearSampler, uvBL_Color);
    lowColors[3] = g_Color.Sample(LinearSampler, uvBR_Color);

    // Sample depths
	float4 LowDepths;
    LowDepths.x = g_LowDepth.Sample(PointSampler, uvTL_Color);
    LowDepths.y = g_LowDepth.Sample(PointSampler, uvTR_Color);
    LowDepths.z = g_LowDepth.Sample(PointSampler, uvBL_Color);
    LowDepths.w = g_LowDepth.Sample(PointSampler, uvBR_Color);

    float refDepth = g_Depth.Sample(PointSampler, uv);

	float3 lowNormals[4];
	lowNormals[0] = normalize(g_Normal.Sample(PointSampler, uvTL_Color).xyz);
	lowNormals[1] = normalize(g_Normal.Sample(PointSampler, uvTR_Color).xyz);
	lowNormals[2] = normalize(g_Normal.Sample(PointSampler, uvBL_Color).xyz);
	lowNormals[3] = normalize(g_Normal.Sample(PointSampler, uvBR_Color).xyz);

	float3 refNormal = normalize(g_Normal.Sample(PointSampler, uv).xyz);

	return BilateralUpsample(refDepth, refNormal, LowDepths, lowNormals, lowColors);

}