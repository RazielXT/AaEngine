#include "hlsl/common/DataPacking.hlsl"

struct AnisoSceneVoxelChunkInfo
{
	float3 Offset;
	float Density;
	float3 MoveOffset;
	float WorldSize;
	// Packed face texture IDs to match C++ layout (uint arrays don't pack tightly in cbuffers)
	uint4 TexIdData0; // texIdFaces[0..3]: +X, -X, +Y, -Y
	uint4 TexIdData1; // texIdFaces[4..5]: +Z, -Z | texIdPrevFaces[0..1]: +X, -X
	uint4 TexIdData2; // texIdPrevFaces[2..5]: +Y, -Y, +Z, -Z
	uint4 TexIdData3; // resIdDataBuffer, padding[3]
};

struct AnisoSceneVoxelCbufferIndexed
{
	float2 MiddleConeRatio;
	float2 SideConeRatio;

	AnisoSceneVoxelChunkInfo Voxels[4];
};

// Face indices: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
uint GetFaceTexId(AnisoSceneVoxelChunkInfo info, uint faceIndex)
{
	if (faceIndex < 4) return info.TexIdData0[faceIndex];
	return info.TexIdData1[faceIndex - 4];
}

uint GetPrevFaceTexId(AnisoSceneVoxelChunkInfo info, uint faceIndex)
{
	if (faceIndex < 2) return info.TexIdData1[faceIndex + 2];
	return info.TexIdData2[faceIndex - 2];
}

uint GetResIdData(AnisoSceneVoxelChunkInfo info)
{
	return info.TexIdData3.x;
}

float4 SampleAnisoVoxel(float3 dir, float3 pos, float lod,
	Texture3D facePosX, Texture3D faceNegX,
	Texture3D facePosY, Texture3D faceNegY,
	Texture3D facePosZ, Texture3D faceNegZ,
	SamplerState sampl)
{
	// Convention: Face +X stores radiance of surfaces with n.x > 0 (visible from +X)
	// When tracing in direction d, we approach from -d side:
	//   tracing +X (d.x>0) → approaching from -X → see -X facing surfaces → sample FaceNegX
	float3 dirPos = max(0, dir);
	float3 dirNeg = max(0, -dir);

	float4 sample =
		dirPos.x * faceNegX.SampleLevel(sampl, pos, lod) +
		dirNeg.x * facePosX.SampleLevel(sampl, pos, lod) +
		dirPos.y * faceNegY.SampleLevel(sampl, pos, lod) +
		dirNeg.y * facePosY.SampleLevel(sampl, pos, lod) +
		dirPos.z * faceNegZ.SampleLevel(sampl, pos, lod) +
		dirNeg.z * facePosZ.SampleLevel(sampl, pos, lod);

	return sample;
}

float4 AnisoConeTrace(float3 o, float3 d, float coneRatio, float maxDist,
	Texture3D facePosX, Texture3D faceNegX,
	Texture3D facePosY, Texture3D faceNegY,
	Texture3D facePosZ, Texture3D faceNegZ,
	SamplerState sampl)
{
	const float voxDim = 128.0f;
	const float minDiam = 1.0 / voxDim;
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
			facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ, sampl);

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
