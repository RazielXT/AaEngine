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
	uint OutputQueueIndex;
	uint IsLastCascade;
	uint Padding0;
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
	float3 baseRayStart = ReconstructDeferredVrtWorldPosition(pixel, ViewportSize, ray.depth, InvViewProjectionMatrix) + worldNormal;

	float3 currentStart = baseRayStart + ray.rayDirection * ray.tCurrent;

	AnisoSeparateSceneVoxelChunkInfo cascade = VoxelInfo.Voxels[CascadeIndex];
	float3 localUV = (currentStart - cascade.Offset) / cascade.WorldSize;

	bool bKeepTracing = true;
	bool bOutOfBounds = any(localUV < 0.0f) || any(localUV > 1.0f);

	if (bOutOfBounds)
	{
		if (IsLastCascade)
		{
			RayResults[pixelIndex].radianceOcclusion = DeferredVrtMissResult(ray.rayDirection);
			return;
		}
		bKeepTracing = false; // Needs to stream to next cascade immediately
	}

	if (bKeepTracing)
	{
		if (CascadeIndex == 0)
			currentStart += ray.rayDirection * 0.3f;

		RayTraceResult traceResult = RayTraceSingle(currentStart, ray.rayDirection, 0, cascade);

		if (traceResult.hit)
		{
			Texture3D<float4> facePosX = GetTexture3D(GetFaceTexId(cascade, 0));
			Texture3D<float4> faceNegX = GetTexture3D(GetFaceTexId(cascade, 1));
			Texture3D<float4> facePosY = GetTexture3D(GetFaceTexId(cascade, 2));
			Texture3D<float4> faceNegY = GetTexture3D(GetFaceTexId(cascade, 3));
			Texture3D<float4> facePosZ = GetTexture3D(GetFaceTexId(cascade, 4));
			Texture3D<float4> faceNegZ = GetTexture3D(GetFaceTexId(cascade, 5));

			int4 loadCoord = int4(traceResult.hitVoxelCoord, 0);
			float3 blendedColor = SampleAnisotropicColor(
				loadCoord, ray.rayDirection,
				facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ,
				traceResult.hitAxis);

			RayResults[pixelIndex].radianceOcclusion = DeferredVrtHitResult(blendedColor);
			return;
		}

		if (IsLastCascade)
		{
			RayResults[pixelIndex].radianceOcclusion = DeferredVrtMissResult(ray.rayDirection);
			return;
		}

		// OPTIMIZATION: Replaced expensive length() square-root with a simple dot product
		ray.tCurrent = dot(traceResult.exitWorldPos - baseRayStart, ray.rayDirection);
	}

	// --- WAVE-COALESCED COMPACTION COMPONENT ---
	// Everyone left in this shader needs to be pushed to the next cascade output queue
	bool bNeedsToAppend = true; 

	uint waveCount   = WaveActiveCountBits(bNeedsToAppend);
	uint waveOffset  = WavePrefixCountBits(bNeedsToAppend);
	uint laneIdx     = WaveGetLaneIndex();
	uint leaderLane  = WaveActiveMin(laneIdx);

	uint globalBaseIndex = 0;
	if (laneIdx == leaderLane)
	{
		// A single atomic instruction handles the entire wavefront
		InterlockedAdd(QueueState[OutputQueueIndex], waveCount, globalBaseIndex);

		// Safely update dispatch args just once per wave
		uint nextGroupCount = (globalBaseIndex + waveCount + DEFERRED_VRT_TRACE_THREADS - 1) / DEFERRED_VRT_TRACE_THREADS;
		InterlockedMax(DispatchArgs[0], nextGroupCount);
	}

	// Broadcast the allocated memory address to all lanes in the wave
	globalBaseIndex = WaveReadLaneAt(globalBaseIndex, leaderLane);

	// Tightly pack the surviving ray into the output buffer
	OutputRays[globalBaseIndex + waveOffset] = ray;
}