#include "deferredVRT_computeCommon.hlsl"

cbuffer CB0 : register(b0)
{
	float4x4 InvViewProjectionMatrix;
	float3 CameraPosition;
	float Time;
	uint2 ViewportSize;
	uint TexIdNormal;
	uint CascadeIndex;
	uint InputQueueIndex;
	uint HitQueueIndex;
	uint BypassQueueIndex;
	uint IsLastCascade;
	uint CoarseMip;
	uint Padding0;
	uint Padding1;
	uint Padding2;
};

cbuffer SceneVoxelInfo : register(b1)
{
	AnisoSeparateSceneVoxelCbufferIndexed VoxelInfo;
};

StructuredBuffer<DeferredVrtRayData> InputRays : register(t0);

RWStructuredBuffer<DeferredVrtRayData> HitRays : register(u0);
RWStructuredBuffer<DeferredVrtRayData> BypassRays : register(u1);
RWStructuredBuffer<DeferredVrtRayResult> RayResults : register(u2);
RWStructuredBuffer<uint> QueueState : register(u3);
RWStructuredBuffer<uint> HitDispatchArgs : register(u4);
RWStructuredBuffer<uint> BypassDispatchArgs : register(u5);

void EnqueueHitRay(DeferredVrtRayData ray)
{
	uint queueIndex;
	InterlockedAdd(QueueState[HitQueueIndex], 1, queueIndex);
	HitRays[queueIndex] = ray;

	uint groupCount = (queueIndex + DEFERRED_VRT_TRACE_THREADS) / DEFERRED_VRT_TRACE_THREADS;
	InterlockedMax(HitDispatchArgs[0], groupCount);
	HitDispatchArgs[1] = 1;
	HitDispatchArgs[2] = 1;
}

void EnqueueBypassRay(DeferredVrtRayData ray)
{
	uint queueIndex;
	InterlockedAdd(QueueState[BypassQueueIndex], 1, queueIndex);
	BypassRays[queueIndex] = ray;

	uint groupCount = (queueIndex + DEFERRED_VRT_TRACE_THREADS) / DEFERRED_VRT_TRACE_THREADS;
	InterlockedMax(BypassDispatchArgs[0], groupCount);
	BypassDispatchArgs[1] = 1;
	BypassDispatchArgs[2] = 1;
}

[numthreads(DEFERRED_VRT_TRACE_THREADS, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint inputCount = QueueState[InputQueueIndex];
	uint rayIndex = DTid.x;
	if (rayIndex >= inputCount)
		return;

	DeferredVrtRayData ray = InputRays[rayIndex];
	uint2 pixel = UnpackPixel(ray.packedData);
	uint pixelIndex = PixelIndex(pixel, ViewportSize);
	float3 worldNormal = LoadDeferredVrtNormal(pixel, ViewportSize, TexIdNormal);
	float3 baseRayStart = ReconstructDeferredVrtWorldPosition(pixel, ViewportSize, ray.depth, InvViewProjectionMatrix) + worldNormal;
	float3 currentStart = baseRayStart + ray.rayDirection * ray.tCurrent;

	AnisoSeparateSceneVoxelChunkInfo cascade = VoxelInfo.Voxels[CascadeIndex];
	float3 localUV = (currentStart - cascade.Offset) / cascade.WorldSize;

	if (any(localUV < 0.0f) || any(localUV > 1.0f))
	{
		if (IsLastCascade)
			RayResults[pixelIndex].radianceOcclusion = DeferredVrtMissResult(ray.rayDirection);
		else
			EnqueueBypassRay(ray);
		return;
	}

	RayTraceResult traceResult = RayTraceSingle(currentStart, ray.rayDirection, CoarseMip, cascade);
	ray.tCurrent = max(ray.tCurrent, length(traceResult.exitWorldPos - baseRayStart));

	if (traceResult.hit)
	{
		EnqueueHitRay(ray);
		return;
	}

	if (IsLastCascade)
	{
		RayResults[pixelIndex].radianceOcclusion = DeferredVrtMissResult(ray.rayDirection);
		return;
	}

	EnqueueBypassRay(ray);
}