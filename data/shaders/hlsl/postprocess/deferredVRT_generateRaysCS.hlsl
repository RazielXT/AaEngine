#include "deferredVRT_computeCommon.hlsl"

cbuffer CB0 : register(b0)
{
	float4x4 InvViewProjectionMatrix;
	float3 CameraPosition;
	float Time;
	uint2 ViewportSize;
	uint TexIdNormal;
	uint TexIdDepth;
	uint RayIndex;
	uint RayCount;
	uint OutputQueueIndex;
	uint Padding0;
};

RWStructuredBuffer<DeferredVrtRayData> OutputRays : register(u0);
RWStructuredBuffer<DeferredVrtRayResult> RayResults : register(u1);
RWStructuredBuffer<uint> QueueState : register(u2);
RWStructuredBuffer<uint> DispatchArgs : register(u3);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 pixel = DTid.xy;
	if (any(pixel >= ViewportSize))
		return;

	uint pixelIndex = PixelIndex(pixel, ViewportSize);
	float3 worldNormal = LoadDeferredVrtNormal(pixel, ViewportSize, TexIdNormal);
	float depth = LoadDeferredVrtDepth(pixel, ViewportSize, TexIdDepth);

	if (all(worldNormal == float3(0, 0, 0)))
	{
		RayResults[pixelIndex].radianceOcclusion = float4(0, 0, 0, 1);
		return;
	}

	float3 up = abs(worldNormal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
	float3 worldTangent = normalize(cross(up, worldNormal));
	float3 worldBinormal = cross(worldNormal, worldTangent);

	float2 uv = (float2(pixel) + 0.5f) / float2(ViewportSize);
	float2 xi = Random2DFrom2D(uv + Time % 10 + float2(RayIndex * 0.239, RayIndex * 0.137));
	float2 xi2 = Random2DFrom2D(uv - Time % 3 + float2(RayIndex * 0.119, RayIndex * 0.437));
	float2 noiseWeight = float2(xi.x, xi2.y);

	// Instead of entirely random values, offset each ray into its own quadrant
	// Assuming RayIndex goes from 0 to 3
	float2 quadrantOffset = float2(float(RayIndex % 2) * 0.5f, float(RayIndex / 2) * 0.5f);

	// Remap your blue noise weight (0.0 to 1.0) into a tight 0.0 to 0.5 range, 
	// then slide it into its assigned quadrant
	float2 stratifiedXi = quadrantOffset + (noiseWeight * 0.5f);

	// Feed this perfectly distributed coordinate into your hemisphere function
	float3 rayDirection = CosineWeightedHemisphere(stratifiedXi, worldNormal, worldTangent, worldBinormal);

	uint queueIndex;
	InterlockedAdd(QueueState[OutputQueueIndex], 1, queueIndex);

	DeferredVrtRayData ray;
	ray.packedData = PackPixel(pixel);
	ray.packedNormal = PackR11G10B11_SNORM(worldNormal);
	ray.depth = depth;
	ray.tCurrent = 0.0f;
	ray.rayDirection = rayDirection;
	OutputRays[queueIndex] = ray;

	uint groupCount = (queueIndex + DEFERRED_VRT_TRACE_THREADS) / DEFERRED_VRT_TRACE_THREADS;
	InterlockedMax(DispatchArgs[0], groupCount);
	DispatchArgs[1] = 1;
	DispatchArgs[2] = 1;
}