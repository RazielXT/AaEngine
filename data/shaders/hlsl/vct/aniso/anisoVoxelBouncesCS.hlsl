#include "AnisoVoxelConeTracingCommon.hlsl"

float3 CameraPosition;
float _pad0;
float3 VoxelsOffset;
float _pad1;
float3 VoxelsSceneSize;

StructuredBuffer<VoxelSceneData> VoxelData : register(t0);

// 6 previous face textures for reading (bound as SRV descriptor tables)
Texture3D<float4> PrevFacePosX : register(t1);
Texture3D<float4> PrevFaceNegX : register(t2);
Texture3D<float4> PrevFacePosY : register(t3);
Texture3D<float4> PrevFaceNegY : register(t4);
Texture3D<float4> PrevFacePosZ : register(t5);
Texture3D<float4> PrevFaceNegZ : register(t6);

// 6 current face textures for writing (bound as UAV descriptor tables)
RWTexture3D<float4> FacePosX : register(u0);
RWTexture3D<float4> FaceNegX : register(u1);
RWTexture3D<float4> FacePosY : register(u2);
RWTexture3D<float4> FaceNegY : register(u3);
RWTexture3D<float4> FacePosZ : register(u4);
RWTexture3D<float4> FaceNegZ : register(u5);

SamplerState VoxelSampler : register(s0);

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
	float3 baseColor = diffuse.xyz;
	float3 worldNormal = UnpackR11G10B11_SNORM(VoxelData[linearIndex].Normal).xyz;
	float3 voxelUV = (float3(id) + 0.5f) / 128;

	float4 traceSample = AnisoConeTrace(voxelUV, worldNormal, 1.05f, 1.5f,
		PrevFacePosX, PrevFaceNegX, PrevFacePosY, PrevFaceNegY, PrevFacePosZ, PrevFaceNegZ,
		VoxelSampler);

	float3 bounceColor = baseColor * traceSample.rgb * 0.5f;
	float shadow = diffuse.w;
	bounceColor += baseColor * shadow * 0.5f;

	float3 currentLightBounce = bounceColor;

	float3 camOffset = (CameraPosition - VoxelsOffset) / VoxelsSceneSize;
	float3 diffOffset = abs(camOffset - voxelUV) * 2;
	float3 distanceFade = saturate(-4 * (diffOffset - 0.5) + 1);
	float fade = min(min(distanceFade.x, distanceFade.y), distanceFade.z);

	// Distribute bounced light to face textures weighted by surface normal
	float3 nPos = max(0, worldNormal);
	float3 nNeg = max(0, -worldNormal);

	FacePosX[id] = float4(currentLightBounce * nPos.x, 1) * fade;
	FaceNegX[id] = float4(currentLightBounce * nNeg.x, 1) * fade;
	FacePosY[id] = float4(currentLightBounce * nPos.y, 1) * fade;
	FaceNegY[id] = float4(currentLightBounce * nNeg.y, 1) * fade;
	FacePosZ[id] = float4(currentLightBounce * nPos.z, 1) * fade;
	FaceNegZ[id] = float4(currentLightBounce * nNeg.z, 1) * fade;
}
