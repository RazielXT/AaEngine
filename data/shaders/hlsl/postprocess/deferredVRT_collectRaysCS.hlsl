#include "deferredVRT_computeCommon.hlsl"

cbuffer CB0 : register(b0)
{
	uint2 ViewportSize;
	uint ResIdOutput;
	uint Padding0;
};

StructuredBuffer<DeferredVrtRayResult> RayResults : register(t0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 pixel = DTid.xy;
	if (any(pixel >= ViewportSize))
		return;

	RWTexture2D<float4> Output = ResourceDescriptorHeap[ResIdOutput];
	Output[pixel] = RayResults[PixelIndex(pixel, ViewportSize)].radianceOcclusion;
}