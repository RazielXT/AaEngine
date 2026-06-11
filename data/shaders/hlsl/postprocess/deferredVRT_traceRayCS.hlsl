#include "deferredVRT_computeCommon.hlsl"

cbuffer CB0 : register(b0)
{
	float4x4 InvViewProjectionMatrix;
	float3 CameraPosition;
	float Time;
	uint2 ViewportSize;
	uint TexIdNormal;
	uint TexIdDepth;
	uint CascadeIndex;
	uint InputQueueIndex;
	uint OutputQueueIndex;
	uint IsLastCascade;
};

cbuffer SceneVoxelInfo : register(b1)
{
	AnisoSeparateSceneVoxelCbufferIndexed VoxelInfo;
};

StructuredBuffer<DeferredVrtRayData> InputRays : register(t0);

RWStructuredBuffer<DeferredVrtRayData> OutputRays : register(u0);
RWStructuredBuffer<DeferredVrtRayResult> RayResults : register(u1);
RWStructuredBuffer<uint> QueueState : register(u2);
RWStructuredBuffer<uint> DispatchArgs : register(u3);

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
	float3 baseRayStart = ReconstructDeferredVrtWorldPosition(pixel, ViewportSize, TexIdDepth, InvViewProjectionMatrix) + worldNormal;
	float3 currentStart = baseRayStart + ray.rayDirection * ray.tCurrent;

	AnisoSeparateSceneVoxelChunkInfo cascade = VoxelInfo.Voxels[CascadeIndex];
	float3 localUV = (currentStart - cascade.Offset) / cascade.WorldSize;

	if (any(localUV < 0.0f) || any(localUV > 1.0f))
	{
		if (IsLastCascade)
		{
			RayResults[pixelIndex].radianceOcclusion = DeferredVrtMissResult(ray.rayDirection);
		}
		else
		{
			uint queueIndex;
			InterlockedAdd(QueueState[OutputQueueIndex], 1, queueIndex);
			OutputRays[queueIndex] = ray;

			uint groupCount = (queueIndex + DEFERRED_VRT_TRACE_THREADS) / DEFERRED_VRT_TRACE_THREADS;
			InterlockedMax(DispatchArgs[0], groupCount);
			DispatchArgs[1] = 1;
			DispatchArgs[2] = 1;
		}
		return;
	}

	if (CascadeIndex == 0)
		currentStart += ray.rayDirection * 0.3f;

	Texture3D<float4> facePosX = GetTexture3D(GetFaceTexId(cascade, 0));
	Texture3D<float4> faceNegX = GetTexture3D(GetFaceTexId(cascade, 1));
	Texture3D<float4> facePosY = GetTexture3D(GetFaceTexId(cascade, 2));
	Texture3D<float4> faceNegY = GetTexture3D(GetFaceTexId(cascade, 3));
	Texture3D<float4> facePosZ = GetTexture3D(GetFaceTexId(cascade, 4));
	Texture3D<float4> faceNegZ = GetTexture3D(GetFaceTexId(cascade, 5));

	RayTraceResult traceResult = RayTraceSingle(currentStart, ray.rayDirection, 0, cascade);

	if (traceResult.hit)
	{
		int4 loadCoord = int4(traceResult.hitVoxelCoord, 0);
		float3 blendedColor = SampleAnisotropicColor(
			loadCoord,
			ray.rayDirection,
			facePosX, faceNegX,
			facePosY, faceNegY,
			facePosZ, faceNegZ,
			traceResult.hitAxis);

		RayResults[pixelIndex].radianceOcclusion = DeferredVrtHitResult(blendedColor);
		return;
	}

	if (IsLastCascade)
	{
		RayResults[pixelIndex].radianceOcclusion = DeferredVrtMissResult(ray.rayDirection);
		return;
	}

	float nextT = length(traceResult.exitWorldPos - baseRayStart);
	ray.tCurrent = nextT;

	uint nextQueueIndex;
	InterlockedAdd(QueueState[OutputQueueIndex], 1, nextQueueIndex);
	OutputRays[nextQueueIndex] = ray;

	uint nextGroupCount = (nextQueueIndex + DEFERRED_VRT_TRACE_THREADS) / DEFERRED_VRT_TRACE_THREADS;
	InterlockedMax(DispatchArgs[0], nextGroupCount);
	DispatchArgs[1] = 1;
	DispatchArgs[2] = 1;
}