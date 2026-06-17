#include "deferredVRT_computeCommon.hlsl"

cbuffer CB0 : register(b0)
{
	uint2 ViewportSize;
	uint Padding0;
	uint Padding1;
};

RWStructuredBuffer<DeferredVrtRayResult> AccumulatedResults : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 pixel = DTid.xy;
	if (any(pixel >= ViewportSize))
		return;

	AccumulatedResults[PixelIndex(pixel, ViewportSize)].radianceOcclusion = 0.0f;
}