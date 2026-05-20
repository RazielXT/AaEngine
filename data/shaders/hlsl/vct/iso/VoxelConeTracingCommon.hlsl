#pragma once

#include "hlsl/common/DataPacking.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"

struct SceneVoxelChunkInfo
{
	float3 Offset;
	float Density;
	float3 MoveOffset;
	float WorldSize;
	uint TexId;
	uint TexIdPrev;
	uint ResIdData;
};

struct SceneVoxelCbuffer
{
	float2 MiddleConeRatio;
	float2 SideConeRatio;

	SceneVoxelChunkInfo NearVoxels;
	SceneVoxelChunkInfo FarVoxels;
};

struct SceneVoxelCbufferIndexed
{
	float2 MiddleConeRatio;
	float2 SideConeRatio;

	SceneVoxelChunkInfo Voxels[4];
};

float4 SampleVoxel(Texture3D cmap, SamplerState sampl, float3 pos, float lod)
{
	return cmap.SampleLevel(sampl, pos, lod);
}

float4 ConeTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl)
{
	const float voxDim = 128.0f;
	const float minDiam = 1.0 / voxDim;
	const float startDist = minDiam;
	float dist = startDist;
	float3 samplePos = o;
	float4 accum = float4(0, 0, 0, 0);

	[loop]
	while (dist <= maxDist && accum.w < 1.0)
	{
		float sampleDiam = max(minDiam, coneRatio * dist);
		float sampleLOD = max(0, log2(sampleDiam * voxDim));
		float3 samplePos = o + d * dist;
		float4 voxel = SampleVoxel(voxelTexture, sampl, samplePos, sampleLOD);

		float sampleWt = (1.0 - accum.w);
		accum += voxel * sampleWt;

		dist += sampleDiam * 0.5;
	}

	accum.xyz *= accum.w;

	return accum;
}

struct VoxelSceneData
{
	uint Diffuse; //PackRGBA8
	uint Normal; //PackR11G10B11_SNORM
};

float4 ConeTraceCascades(float3 worldPos, float3 d, float coneRatio, SceneVoxelCbufferIndexed voxelInfo, SamplerState sampl)
{
	const float voxDim = 128.0f;
	const float minDiam = 1.0 / voxDim;
	float4 accum = float4(0, 0, 0, 0);
	float totalWorldDist = 0;

	if (d.x == 0) d.x = 1e-6;
	if (d.y == 0) d.y = 1e-6;
	if (d.z == 0) d.z = 1e-6;

	for (int c = 0; c < 4; c++)
	{
		if (accum.w >= 1.0) break;

		SceneVoxelChunkInfo cascade = voxelInfo.Voxels[c];

		// Current world position along the ray
		float3 currentWorldPos = worldPos + d * totalWorldDist;

		// Transform to cascade UV space
		float3 o = (currentWorldPos - cascade.Offset) / cascade.WorldSize;

		// Skip if outside this cascade
		if (any(o < 0.0) || any(o > 1.0)) continue;

		Texture3D voxelTex = GetTexture3D(cascade.TexId);

		// Position distance starts at origin, cone distance preserves accumulated size
		float dist = minDiam;
		float coneDist = max(minDiam, totalWorldDist / cascade.WorldSize);

		// Compute max UV distance to exit cascade boundary
		float3 tExit = (step(0, d) - o) / d;
		float maxDist = min(min(tExit.x, tExit.y), tExit.z);

		[loop]
		while (dist <= maxDist && accum.w < 1.0)
		{
			float sampleDiam = max(minDiam, coneRatio * coneDist);
			float sampleLOD = max(0, log2(sampleDiam * voxDim));
			float3 samplePos = o + d * dist;
			float4 voxel = SampleVoxel(voxelTex, sampl, samplePos, sampleLOD);

			float sampleWt = (1.0 - accum.w);
			accum += voxel * sampleWt;

			float stepSize = sampleDiam * 0.5;
			dist += stepSize;
			coneDist += stepSize;
		}

		// Update total world distance for next cascade
		totalWorldDist += dist * cascade.WorldSize;
	}

	accum.xyz *= accum.w;

	return accum;
}
