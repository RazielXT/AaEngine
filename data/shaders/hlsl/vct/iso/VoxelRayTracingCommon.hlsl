#pragma once

#include "VoxelConeTracingCommon.hlsl"

struct RayTraceResult
{
	float4 color;
	float3 exitWorldPos;
	bool hit;
};

// Simple Ray-AABB intersection helper to get our exit T
float GetRayAABBExitT(float3 rayStart, float3 rayDir, float3 boxMin, float3 boxMax)
{
	// Use select() for component-wise vector conditional checks
	float3 safeRayDir = select(abs(rayDir) < 1e-6f, sign(rayDir) * 1e-6f, rayDir);
	float3 invDir = 1.0f / safeRayDir;

	float3 t0 = (boxMin - rayStart) * invDir;
	float3 t1 = (boxMax - rayStart) * invDir;

	float3 tMax = max(t0, t1);

	// Return the minimum of the maximum components
	return min(tMax.x, min(tMax.y, tMax.z));
	}

RayTraceResult RayTraceSingle(float3 rayStart, float3 rayDir, uint voxelMip, SceneVoxelChunkInfo voxels, Texture3D voxelmap)
{
	const float3 voxelSize = float(1u << voxelMip) / voxels.Density;
	const float3 VoxelDimensions = voxels.Density * voxels.WorldSize;

	rayStart += voxelSize * rayDir * 1.5f;

	RayTraceResult result;
	result.color = float4(0, 0, 0, 0);
	result.exitWorldPos = rayStart;
	result.hit = false;

	// Precalculate safe inverse direction and step signs
	float3 invDir = 1.0f / select(abs(rayDir) < 1e-6f, sign(rayDir) * 1e-6f, rayDir);
	int3 stepDir = int3(sign(rayDir));

	// Calculate exact exit T for the entire chunk bounds to kill inner bounds checking
	float3 chunkMin = voxels.Offset;
	float3 chunkMax = voxels.Offset + voxels.WorldSize;
	float tExitGrid = GetRayAABBExitT(rayStart, rayDir, chunkMin, chunkMax);

	int3 voxelCoord = int3(floor((rayStart - voxels.Offset) / voxelSize));

	float3 stepSign = float3(rayDir.x >= 0.0f ? 1.0f : 0.0f, rayDir.y >= 0.0f ? 1.0f : 0.0f, rayDir.z >= 0.0f ? 1.0f : 0.0f);
	float3 nextBoundary = (float3(voxelCoord) + stepSign) * voxelSize + voxels.Offset;

	float3 tMax = (nextBoundary - rayStart) * invDir;
	float3 tDelta = voxelSize * abs(invDir);
	float exitT = 0;

	[loop]
	for (int i = 0; i < 256; i++)
	{
		// DDA Step
		if (tMax.x < tMax.y && tMax.x < tMax.z)
		{
			exitT = tMax.x;
			voxelCoord.x += stepDir.x;
			tMax.x += tDelta.x;
		}
		else if (tMax.y < tMax.z)
		{
			exitT = tMax.y;
			voxelCoord.y += stepDir.y;
			tMax.y += tDelta.y;
		}
		else
		{
			exitT = tMax.z;
			voxelCoord.z += stepDir.z;
			tMax.z += tDelta.z;
		}

		// Combined escape condition: out of max steps or out of bounds
		if (exitT >= tExitGrid)
			break;

		// Fetching from a single channel R8 texture (Lightweight!)
		float4 voxelColor = voxelmap.Load(int4(voxelCoord, voxelMip));

		if (voxelColor.w > 0.0f)
		{
			result.color = voxelColor;
			result.hit = true;
			break;
		}
	}

	result.exitWorldPos = rayStart + rayDir * exitT;
	return result;
}

float4 RayTraceCascades(float3 rayStart, float3 rayDir, uint voxelMip, SceneVoxelCbufferIndexed voxelInfo)
{
	float3 currentStart = rayStart;

	for (int c = 0; c < 4; c++)
	{
		SceneVoxelChunkInfo cascade = voxelInfo.Voxels[c];
		float3 localUV = (currentStart - cascade.Offset) / cascade.WorldSize;

		if (any(localUV < 0.0) || any(localUV > 1.0))
			continue;

		Texture3D voxelmap = GetTexture3D(cascade.TexId);
		RayTraceResult result = RayTraceSingle(currentStart, rayDir, voxelMip, cascade, voxelmap);

		if (result.hit)
			return result.color;

		currentStart = result.exitWorldPos;
	}

	return float4(0, 0, 0, 0);
}
