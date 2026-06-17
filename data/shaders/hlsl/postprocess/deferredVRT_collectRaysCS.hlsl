#include "deferredVRT_computeCommon.hlsl"

cbuffer CB0 : register(b0)
{
	uint2 ViewportSize;
	uint ResIdOutput;
	uint RayCount;
	uint IsLastRay;
	uint Padding0;
};

StructuredBuffer<DeferredVrtRayResult> RayResults : register(t0);
RWStructuredBuffer<DeferredVrtRayResult> AccumulatedResults : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 pixel = DTid.xy;
	if (any(pixel >= ViewportSize))
		return;

	uint pixelIndex = PixelIndex(pixel, ViewportSize);
	float4 accumulated = AccumulatedResults[pixelIndex].radianceOcclusion + RayResults[pixelIndex].radianceOcclusion;
	AccumulatedResults[pixelIndex].radianceOcclusion = accumulated;

	if (IsLastRay)
	{
		RWTexture2D<float4> Output = ResourceDescriptorHeap[ResIdOutput];
		Output[pixel] = accumulated / float(max(1u, RayCount));
	}
}