#include "AnisoSeparateVoxelConeTracingCommon.hlsl"

float3 SunColor;

StructuredBuffer<VoxelSceneData> VoxelData : register(t0);

/*
Texture3D<float4> PrevFacePosX : register(t1);
Texture3D<float4> PrevFaceNegX : register(t2);
Texture3D<float4> PrevFacePosY : register(t3);
Texture3D<float4> PrevFaceNegY : register(t4);
Texture3D<float4> PrevFacePosZ : register(t5);
Texture3D<float4> PrevFaceNegZ : register(t6);
Texture3D<float> PrevOccupancy : register(t7);

SamplerState VoxelSampler : register(s0);
*/

RWTexture3D<float3> FacePosX : register(u0);
RWTexture3D<float3> FaceNegX : register(u1);
RWTexture3D<float3> FacePosY : register(u2);
RWTexture3D<float3> FaceNegY : register(u3);
RWTexture3D<float3> FacePosZ : register(u4);
RWTexture3D<float3> FaceNegZ : register(u5);
RWTexture3D<float> Occupancy : register(u6);

[numthreads(4,4,4)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint linearIndex = id.z * (128 * 128) + id.y * 128 + id.x;

	bool isEmpty = VoxelData[linearIndex].Normal == 0;

	if (WaveActiveAllTrue(isEmpty))
		return;
	if (isEmpty)
		return;

	float4 diffuse = UnpackRGBA8(VoxelData[linearIndex].Diffuse);
	float3 baseColor = diffuse.yzw;
	float4 data2 = UnpackRGBA8(VoxelData[linearIndex].Normal);
	float3 worldNormal = data2.xyz;

	float shadow = saturate(diffuse.x);
	float emmisive = data2.w * 5;
	float3 directColor = baseColor * shadow * SunColor + baseColor * emmisive;

/*
	float3 voxelUV = (float3(id) + 0.5f) / 128;
	float4 traceSample = AnisoConeTrace(voxelUV, worldNormal, 1.05f, 1.5f,
		PrevFacePosX, PrevFaceNegX, PrevFacePosY, PrevFaceNegY, PrevFacePosZ, PrevFaceNegZ,
		PrevOccupancy,
		VoxelSampler);

	float3 bounceColor = baseColor * traceSample.rgb * 0.5;
	directColor += bounceColor;
*/

	float3 currentLightBounce = directColor;

	float3 nPos = max(saturate(emmisive), worldNormal);
	float3 nNeg = max(saturate(emmisive), -worldNormal);

	FacePosX[id] = currentLightBounce * nPos.x;
	FaceNegX[id] = currentLightBounce * nNeg.x;
	FacePosY[id] = currentLightBounce * nPos.y;
	FaceNegY[id] = currentLightBounce * nNeg.y;
	FacePosZ[id] = currentLightBounce * nPos.z;
	FaceNegZ[id] = currentLightBounce * nNeg.z;

	Occupancy[id] = 1.0f;
}