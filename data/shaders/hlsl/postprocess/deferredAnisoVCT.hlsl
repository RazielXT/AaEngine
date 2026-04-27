#include "PostProcessCommon.hlsl"
#include "WorldReconstruction.hlsl"
#include "hlsl/vct/aniso/AnisoVoxelConeTracingCommon.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
float ResId;

cbuffer SceneVoxelInfo : register(b1)
{
	AnisoSceneVoxelCbufferIndexed VoxelInfo;
};

Texture2D normalMap : register(t0);
Texture2D<float> depthMap : register(t1);

SamplerState VoxelSampler : register(s0);
SamplerState PointSampler : register(s1);

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float3 worldNormal = normalMap.Sample(PointSampler, input.TexCoord).rgb;
	float3 up = abs(worldNormal.y) < 0.999 ? float3(0,1,0) : float3(1,0,0);
	float3 worldTangent = normalize(cross(up, worldNormal));
	float3 worldBinormal = cross(worldNormal, worldTangent);

	if (all(worldNormal == float3(0,0,0))) return float4(0,0,0,1);

	float depth = depthMap.Sample(PointSampler, input.TexCoord).r;
	float3 worldPosition = ReconstructWorldPosition(input.TexCoord, depth, InvViewProjectionMatrix);

	float3 voxelAmbient = 0;
	float voxelWeight = 1.0f;
	float occlusion = 1.0f;

	for (int idx = 0; idx < 4; idx++)
	{
		float3 voxelUV = (worldPosition - VoxelInfo.Voxels[idx].Offset) / VoxelInfo.Voxels[idx].WorldSize;

		// Resolve 6 face textures from cbuffer IDs
		Texture3D facePosX = GetTexture3D(GetFaceTexId(VoxelInfo.Voxels[idx], 0));
		Texture3D faceNegX = GetTexture3D(GetFaceTexId(VoxelInfo.Voxels[idx], 1));
		Texture3D facePosY = GetTexture3D(GetFaceTexId(VoxelInfo.Voxels[idx], 2));
		Texture3D faceNegY = GetTexture3D(GetFaceTexId(VoxelInfo.Voxels[idx], 3));
		Texture3D facePosZ = GetTexture3D(GetFaceTexId(VoxelInfo.Voxels[idx], 4));
		Texture3D faceNegZ = GetTexture3D(GetFaceTexId(VoxelInfo.Voxels[idx], 5));

		float4 fullTrace = AnisoConeTrace(voxelUV, worldNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y,
			facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ, VoxelSampler);

		{
			fullTrace += AnisoConeTrace(voxelUV, normalize(worldNormal + worldTangent), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y,
				facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ, VoxelSampler);
			fullTrace += AnisoConeTrace(voxelUV, normalize(worldNormal - worldTangent), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y,
				facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ, VoxelSampler);
			fullTrace += AnisoConeTrace(voxelUV, normalize(worldNormal + worldBinormal), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y,
				facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ, VoxelSampler);
			fullTrace += AnisoConeTrace(voxelUV, normalize(worldNormal - worldBinormal), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y,
				facePosX, faceNegX, facePosY, faceNegY, facePosZ, faceNegZ, VoxelSampler);
			fullTrace /= 5;
		}

		voxelAmbient += fullTrace.rgb * voxelWeight;
		voxelWeight = saturate(voxelWeight - fullTrace.w);
		occlusion = min(occlusion, 1 - fullTrace.w);
	}

	return float4(voxelAmbient, occlusion);
}
