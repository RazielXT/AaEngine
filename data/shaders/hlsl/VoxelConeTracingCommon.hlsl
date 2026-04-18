#include "hlsl/utils/dataPacking.hlsl"

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
