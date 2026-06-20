#include "deferredVRT_computeCommon.hlsl"
#include "hlsl/sky/SkyColor.hlsl"

cbuffer CB0 : register(b0)
{
	uint2 ViewportSize;
	uint ResIdOutput;
	uint RayCount;
	uint IsLastRay;
	uint Padding0;
};

cbuffer SkyParamsBuffer : register(b1)
{
	SkyParams Sky;
}

SamplerState LinearSampler : register(s0);

StructuredBuffer<DeferredVrtRayResult> RayResults : register(t0);
RWStructuredBuffer<DeferredVrtRayResult> AccumulatedResults : register(u0);

float3 SkyColor(float3 dir)
{
	float3 sky = getSkyColor(dir, Sky, LinearSampler);

	float t = saturate(dir.y * 0.75 + 0.25);
	float3 bottomColor = float3(0.1, 0.15, 0.2);
	return lerp(bottomColor, sky, 1 - t) * 0.3f;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 pixel = DTid.xy;
	if (any(pixel >= ViewportSize))
		return;

	uint pixelIndex = PixelIndex(pixel, ViewportSize);
	float4 pixelResult = RayResults[pixelIndex].radianceOcclusion;
	pixelResult.rgb = lerp(SkyColor(pixelResult.rgb), pixelResult.rgb, pixelResult.a);

	float4 accumulated = AccumulatedResults[pixelIndex].radianceOcclusion + pixelResult;
	AccumulatedResults[pixelIndex].radianceOcclusion = accumulated;

	if (IsLastRay)
	{
		RWTexture2D<float4> Output = ResourceDescriptorHeap[ResIdOutput];
		Output[pixel] = accumulated / float(max(1u, RayCount));
	}
}