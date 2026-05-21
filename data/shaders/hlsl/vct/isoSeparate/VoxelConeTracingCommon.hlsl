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
	uint TexIdOccupancy;
	uint TexIdPrevOccupancy;
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

float4 SampleVoxel(Texture3D<float4> colorMap, Texture3D<float> occupancyMap, SamplerState sampl, float3 pos, float lod)
{
	float3 color = colorMap.SampleLevel(sampl, pos, lod).rgb;
	float occupancy = occupancyMap.SampleLevel(sampl, pos, lod).r;
	return float4(color, occupancy);
}

float4 ConeTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D<float4> colorTexture, Texture3D<float> occupancyTexture, SamplerState sampl)
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
		float4 voxel = SampleVoxel(colorTexture, occupancyTexture, sampl, samplePos, sampleLOD);

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

		float3 currentWorldPos = worldPos + d * totalWorldDist;
		float3 o = (currentWorldPos - cascade.Offset) / cascade.WorldSize;

		if (any(o < 0.0) || any(o > 1.0)) continue;

		Texture3D<float4> voxelColorTex = GetTexture3D(cascade.TexId);
		Texture3D<float> voxelOccupancyTex = GetTexture3D1f(cascade.TexIdOccupancy);

		float dist = minDiam;
		float coneDist = max(minDiam, totalWorldDist / cascade.WorldSize);

		float3 tExit = (step(0, d) - o) / d;
		float maxDist = min(min(tExit.x, tExit.y), tExit.z);

		[loop]
		while (dist <= maxDist && accum.w < 1.0)
		{
			float sampleDiam = max(minDiam, coneRatio * coneDist);
			float sampleLOD = max(0, log2(sampleDiam * voxDim));
			float3 samplePos = o + d * dist;
			float4 voxel = SampleVoxel(voxelColorTex, voxelOccupancyTex, sampl, samplePos, sampleLOD);

			float sampleWt = (1.0 - accum.w);
			accum += voxel * sampleWt;

			float stepSize = sampleDiam * 0.5;
			dist += stepSize;
			coneDist += stepSize;
		}

		totalWorldDist += dist * cascade.WorldSize;
	}

	accum.xyz *= accum.w;

	return accum;
}