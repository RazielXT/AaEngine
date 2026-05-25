#pragma once

#include "AnisoSeparateVoxelConeTracingCommon.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"

struct RayTraceResult
{
	float4 color;
	float3 exitWorldPos;
	bool hit;
	uint steps;
};

float GetRayAABBExitT(float3 rayStart, float3 rayDir, float3 boxMin, float3 boxMax)
{
	float3 safeRayDir = select(abs(rayDir) < 1e-6f, sign(rayDir) * 1e-6f, rayDir);
	float3 invDir = 1.0f / safeRayDir;

	float3 t0 = (boxMin - rayStart) * invDir;
	float3 t1 = (boxMax - rayStart) * invDir;

	float3 tMax = max(t0, t1);
	return min(tMax.x, min(tMax.y, tMax.z));
}

// Computes a smooth directional blend across the 3 matching faces facing the ray origin
float3 SampleAnisotropicColor(
	int4 voxelCoord,
	float3 rayDir,
	Texture3D<float4> facePosX, Texture3D<float4> faceNegX,
	Texture3D<float4> facePosY, Texture3D<float4> faceNegY,
	Texture3D<float4> facePosZ, Texture3D<float4> faceNegZ)
{
	// Determine weights based on the direction vector components
	float3 weights = abs(rayDir);
	float totalWeight = weights.x + weights.y + weights.z;
	weights /= (totalWeight > 1e-5f) ? totalWeight : 1.0f; // Prevent division by zero

	// Sample the correct side for each of the 3 main axes
	float3 colorX = (rayDir.x > 0.0f) ? faceNegX.Load(voxelCoord).rgb : facePosX.Load(voxelCoord).rgb;
	float3 colorY = (rayDir.y > 0.0f) ? faceNegY.Load(voxelCoord).rgb : facePosY.Load(voxelCoord).rgb;
	float3 colorZ = (rayDir.z > 0.0f) ? faceNegZ.Load(voxelCoord).rgb : facePosZ.Load(voxelCoord).rgb;

	// Perform the 3-axis directional blend
	return (colorX * weights.x) + (colorY * weights.y) + (colorZ * weights.z);
}

#define VRT_RAY_MAX_STEPS 32

RayTraceResult RayTraceSingle(
	float3 rayStart,
	float3 rayDir,
	uint voxelMip,
	AnisoSeparateSceneVoxelChunkInfo voxels,
	Texture3D<float4> facePosX, Texture3D<float4> faceNegX,
	Texture3D<float4> facePosY, Texture3D<float4> faceNegY,
	Texture3D<float4> facePosZ, Texture3D<float4> faceNegZ,
	Texture3D<float> occupancyMap)
{
	const float3 voxelSize = float(1u << voxelMip) / voxels.Density;

	RayTraceResult result;
	result.color = float4(0, 0, 0, 0);
	result.exitWorldPos = rayStart;
	result.hit = false;
	result.steps = 0;

	float3 invDir = 1.0f / select(abs(rayDir) < 1e-6f, sign(rayDir) * 1e-6f, rayDir);
	int3 stepDir = int3(sign(rayDir));

	float3 chunkMin = voxels.Offset;
	float3 chunkMax = voxels.Offset + voxels.WorldSize;
	float tExitGrid = GetRayAABBExitT(rayStart, rayDir, chunkMin, chunkMax);

	int3 voxelCoord = int3(floor((rayStart - voxels.Offset) / voxelSize));

	float3 stepSign = float3(rayDir.x >= 0.0f ? 1.0f : 0.0f, rayDir.y >= 0.0f ? 1.0f : 0.0f, rayDir.z >= 0.0f ? 1.0f : 0.0f);
	float3 nextBoundary = (float3(voxelCoord) + stepSign) * voxelSize + voxels.Offset;

	float3 tMax = (nextBoundary - rayStart) * invDir;
	float3 tDelta = voxelSize * abs(invDir);
	float exitT = 0;

	int3 hitVoxelCoord = int3(0, 0, 0);
	float hitOccupancy = 0.0f;

	[loop]
	for (int i = 0; i < VRT_RAY_MAX_STEPS; i++)
	{
		result.steps++;

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

		if (exitT >= tExitGrid)
			break;

		float occupancy = occupancyMap.Load(int4(voxelCoord, voxelMip)).r;
		if (occupancy > 0.0f)
		{
			hitVoxelCoord = voxelCoord;
			hitOccupancy = occupancy;
			result.hit = true;
			break;
		}
	}

	// Resolve lighting outside of the loop if a hit occurred
	if (result.hit)
	{
		int4 loadCoord = int4(hitVoxelCoord, voxelMip);
		float3 blendedColor = SampleAnisotropicColor(
			loadCoord, 
			rayDir, 
			facePosX, faceNegX, 
			facePosY, faceNegY, 
			facePosZ, faceNegZ
		);

		result.color = float4(blendedColor, hitOccupancy);
	}

	result.exitWorldPos = rayStart + rayDir * exitT;
	return result;
}

float3 HeatmapColor(uint value, uint maxValue)
{
	float t = (maxValue == 0) ? 0.0 : saturate((float)value / (float)maxValue);

	float3 color;
	color.r = saturate(1.5 - abs(4.0 * t - 3.0));
	color.g = saturate(1.5 - abs(4.0 * t - 2.0));
	color.b = saturate(1.5 - abs(4.0 * t - 1.0));

	return color;
}

float4 RayTraceCascades(float3 rayStart, float3 rayDir, uint voxelMip, AnisoSeparateSceneVoxelCbufferIndexed voxelInfo)
{
	float3 currentStart = rayStart;
	bool bOffsetApplied = false;
	uint steps = 0;

	for (int c = 0; c < 4; c++)
	{
		AnisoSeparateSceneVoxelChunkInfo cascade = voxelInfo.Voxels[c];
		float3 localUV = (currentStart - cascade.Offset) / cascade.WorldSize;

		if (any(localUV < 0.0) || any(localUV > 1.0))
			continue;

		if (!bOffsetApplied)
		{
			const float3 voxelSize = float(1u << voxelMip) / cascade.Density;
			float3 localPos = currentStart - cascade.Offset;
			float3 voxelCoord = floor(localPos / voxelSize);
			float3 voxelMin = voxelCoord * voxelSize + cascade.Offset;
			float3 voxelMax = voxelMin + voxelSize;
			float tExitVoxel = GetRayAABBExitT(currentStart, rayDir, voxelMin, voxelMax);

			currentStart = currentStart + rayDir * max(1.0f, tExitVoxel);
			bOffsetApplied = true;

			localUV = (currentStart - cascade.Offset) / cascade.WorldSize;
			if (any(localUV < 0.0) || any(localUV > 1.0))
				continue;
		}

		Texture3D<float4> facePosX = GetTexture3D(GetFaceTexId(cascade, 0));
		Texture3D<float4> faceNegX = GetTexture3D(GetFaceTexId(cascade, 1));
		Texture3D<float4> facePosY = GetTexture3D(GetFaceTexId(cascade, 2));
		Texture3D<float4> faceNegY = GetTexture3D(GetFaceTexId(cascade, 3));
		Texture3D<float4> facePosZ = GetTexture3D(GetFaceTexId(cascade, 4));
		Texture3D<float4> faceNegZ = GetTexture3D(GetFaceTexId(cascade, 5));

		Texture3D<float> occupancyMap = GetTexture3D1f(GetOccupancyTexId(cascade));

		RayTraceResult result = RayTraceSingle(
			currentStart,
			rayDir,
			voxelMip,
			cascade,
			facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ,
			occupancyMap);

		steps += result.steps;

		if (result.hit)
#ifdef VRT_RAY_HEATMAP
			return float4(HeatmapColor(steps, 4 * VRT_RAY_MAX_STEPS), 1);
#else
			return result.color;
#endif

		currentStart = result.exitWorldPos;
	}

#ifdef VRT_RAY_HEATMAP
	return float4(HeatmapColor(steps, 4 * VRT_RAY_MAX_STEPS), 1);
#else
	return float4(0, 0, 0, 0);
#endif
}