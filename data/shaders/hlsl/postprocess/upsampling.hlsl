#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D<float4> g_Color : register(t0);
Texture2D<float>  g_Depth : register(t1);
Texture2D<float>  g_LowDepth : register(t2);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

float4 BilateralUpsample( float HiDepth, float4 LowDepths, float4 LowColors[4] )
{
	const float NoiseFilterStrength = 0.0;
	const float kUpsampleTolerance = 0.000001;

    float4 weights = 1 / ( abs(HiDepth - LowDepths) + kUpsampleTolerance );
    float TotalWeight = dot(weights, 1) + NoiseFilterStrength;

    float4 WeightedSum = NoiseFilterStrength;
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

    // Reference depth (min for edge preservation)
    float refDepth = g_Depth.Sample(PointSampler, uv);//max(max(d0, d1), max(d2, d3));

    return BilateralUpsample(refDepth, LowDepths, lowColors);

}