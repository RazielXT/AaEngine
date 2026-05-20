#pragma once

#include "VoxelConeTracingCommon.hlsl"

struct RayTraceResult
{
	float4 color;
	float3 exitWorldPos;
	bool hit;
};

RayTraceResult RayTraceSingle(float3 rayStart, float3 rayDir, uint voxelMip, SceneVoxelChunkInfo voxels, Texture3D voxelmap)
{
	rayStart -= rayDir * 0.1f;

	RayTraceResult result;
	result.color = float4(0, 0, 0, 0);
	result.exitWorldPos = rayStart;
	result.hit = false;

	if (rayDir.x == 0) rayDir.x = 1e-6;
	if (rayDir.y == 0) rayDir.y = 1e-6;
	if (rayDir.z == 0) rayDir.z = 1e-6;

	const float3 voxelSize = pow(2, voxelMip) / voxels.Density;
	const float3 VoxelDimensions = voxels.Density * voxels.WorldSize;

	int3 voxelCoord = int3(floor((rayStart - voxels.Offset) / voxelSize));

	float3 invDir = 1.0 / rayDir;

	float3 stepSign = float3(
		(rayDir.x > 0.0f) ? 1.0f : 0.0f,
		(rayDir.y > 0.0f) ? 1.0f : 0.0f,
		(rayDir.z > 0.0f) ? 1.0f : 0.0f
	);
	float3 nextBoundary = (float3(voxelCoord) + stepSign) * voxelSize + voxels.Offset;

	float3 tMax = (nextBoundary - rayStart) * invDir;
	float3 tDelta = voxelSize * abs(invDir);

	float exitT = 0;

	[loop]
	for (int i = 0; i < 256; i++)
	{
		// Move first to skip the starting voxel
		if (tMax.x < tMax.y && tMax.x < tMax.z)
		{
			exitT = tMax.x;
			voxelCoord.x += (rayDir.x > 0 ? 1 : -1);
			tMax.x += tDelta.x;
		}
		else if (tMax.y < tMax.z)
		{
			exitT = tMax.y;
			voxelCoord.y += (rayDir.y > 0 ? 1 : -1);
			tMax.y += tDelta.y;
		}
		else
		{
			exitT = tMax.z;
			voxelCoord.z += (rayDir.z > 0 ? 1 : -1);
			tMax.z += tDelta.z;
		}

		// Stop if outside grid
		if (voxelCoord.x < 0 || voxelCoord.x >= VoxelDimensions.x ||
			voxelCoord.y < 0 || voxelCoord.y >= VoxelDimensions.y ||
			voxelCoord.z < 0 || voxelCoord.z >= VoxelDimensions.z)
			break;

		float4 nearVoxel = voxelmap.Load(int4(voxelCoord, voxelMip));

		if (nearVoxel.a > 0.f)
		{
			result.color = float4(nearVoxel.rgb, 1);
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
