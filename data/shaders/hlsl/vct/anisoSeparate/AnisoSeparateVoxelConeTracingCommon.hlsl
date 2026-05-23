#include "hlsl/common/DataPacking.hlsl"

struct AnisoSeparateSceneVoxelChunkInfo
{
	float3 Offset;
	float Density;
	float3 MoveOffset;
	float WorldSize;
	uint4 TexIdData0;
	uint4 TexIdData1;
	uint4 TexIdData2;
	uint4 TexIdData3;
};

struct AnisoSeparateSceneVoxelCbufferIndexed
{
	float2 MiddleConeRatio;
	float2 SideConeRatio;

	AnisoSeparateSceneVoxelChunkInfo Voxels[4];
};

uint GetFaceTexId(AnisoSeparateSceneVoxelChunkInfo info, uint faceIndex)
{
	if (faceIndex < 4) return info.TexIdData0[faceIndex];
	return info.TexIdData1[faceIndex - 4];
}

uint GetPrevFaceTexId(AnisoSeparateSceneVoxelChunkInfo info, uint faceIndex)
{
	if (faceIndex < 2) return info.TexIdData1[faceIndex + 2];
	return info.TexIdData2[faceIndex - 2];
}

uint GetOccupancyTexId(AnisoSeparateSceneVoxelChunkInfo info)
{
	return info.TexIdData3.x;
}

uint GetPrevOccupancyTexId(AnisoSeparateSceneVoxelChunkInfo info)
{
	return info.TexIdData3.y;
}

uint GetResIdData(AnisoSeparateSceneVoxelChunkInfo info)
{
	return info.TexIdData3.z;
}

float4 SampleAnisoVoxel(float3 dir, float3 pos, float lod,
	Texture3D<float4> facePosX, Texture3D<float4> faceNegX,
	Texture3D<float4> facePosY, Texture3D<float4> faceNegY,
	Texture3D<float4> facePosZ, Texture3D<float4> faceNegZ,
	Texture3D<float> occupancy,
	SamplerState sampl)
{
	float3 dirPos = max(0, dir);
	float3 dirNeg = max(0, -dir);

	float3 color =
		dirPos.x * faceNegX.SampleLevel(sampl, pos, lod).rgb +
		dirNeg.x * facePosX.SampleLevel(sampl, pos, lod).rgb +
		dirPos.y * faceNegY.SampleLevel(sampl, pos, lod).rgb +
		dirNeg.y * facePosY.SampleLevel(sampl, pos, lod).rgb +
		dirPos.z * faceNegZ.SampleLevel(sampl, pos, lod).rgb +
		dirNeg.z * facePosZ.SampleLevel(sampl, pos, lod).rgb;

	return float4(color, occupancy.SampleLevel(sampl, pos, lod).r);
}

float4 AnisoConeTrace(float3 o, float3 d, float coneRatio, float maxDist,
	Texture3D<float4> facePosX, Texture3D<float4> faceNegX,
	Texture3D<float4> facePosY, Texture3D<float4> faceNegY,
	Texture3D<float4> facePosZ, Texture3D<float4> faceNegZ,
	Texture3D<float> occupancy,
	SamplerState sampl)
{
	const float voxDim = 128.0f;
	const float minDiam = 1.5 / voxDim;
	const float startDist = minDiam;
	float dist = startDist;
	float4 accum = float4(0, 0, 0, 0);

	[loop]
	while (dist <= maxDist && accum.w < 1.0)
	{
		float sampleDiam = max(minDiam, coneRatio * dist);
		float sampleLOD = max(0, log2(sampleDiam * voxDim));
		float3 samplePos = o + d * dist;

		float4 voxel = SampleAnisoVoxel(d, samplePos, sampleLOD,
			facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ,
			occupancy,
			sampl);

		float sampleWt = (1.0 - accum.w);
		accum += voxel * sampleWt;

		dist += sampleDiam * 0.5;
	}

	accum.xyz *= accum.w;

	return accum;
}

struct VoxelSceneData
{
	uint Diffuse;
	uint Normal;
};