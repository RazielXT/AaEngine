#include "PostProcessCommon.hlsl"
#include "WorldReconstruction.hlsl"
#include "hlsl/VoxelConeTracingCommon.hlsl"

float4x4 InvViewProjectionMatrix;
float ResId;

cbuffer SceneVoxelInfo : register(b1)
{
	SceneVoxelCbufferIndexed VoxelInfo;
};

Texture2D normalMap : register(t0);
Texture2D<float> depthMap : register(t1);

SamplerState VoxelSampler : register(s0);
SamplerState PointSampler : register(s1);

Texture3D<float4> GetTexture3D(uint index)
{
	return ResourceDescriptorHeap[index];
}

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
		Texture3D voxelmap = GetTexture3D(VoxelInfo.Voxels[idx].TexId);

		float4 fullTrace = ConeTraceImpl(voxelUV, worldNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, voxelmap, VoxelSampler);

		//if (idx < 2)
		{
			fullTrace += ConeTraceImpl(voxelUV, normalize(worldNormal + worldTangent), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler);
			fullTrace += ConeTraceImpl(voxelUV, normalize(worldNormal - worldTangent), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler);
			fullTrace += ConeTraceImpl(voxelUV, normalize(worldNormal + worldBinormal), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler);
			fullTrace += ConeTraceImpl(voxelUV, normalize(worldNormal - worldBinormal), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler);
			fullTrace /= 5;
		}

		voxelAmbient += fullTrace.rgb * voxelWeight;
		voxelWeight = saturate(voxelWeight - fullTrace.w);
		occlusion = min(occlusion, 1 - fullTrace.w);
	}


	return float4(voxelAmbient, 1);
}
