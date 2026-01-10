#include "VoxelConeTracingCommon.hlsl"

float3 CameraPosition;
float3 VoxelsOffset;
float3 VoxelsSceneSize;

StructuredBuffer<VoxelSceneData> VoxelData : register(t0);
Texture3D<float4> SceneVoxelBouncesPrev : register(t1);

RWTexture3D<float4> SceneVoxelBounces : register(u0);
 
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
	float3 worldNormal = UnpackRGB10A2_SNORM(VoxelData[linearIndex].Normal).xyz;
	float3 voxelUV = (float3(id) + 0.5f) / 128;

	float4 traceSample = ConeTrace(voxelUV, worldNormal, 1.05f, 1.5f, SceneVoxelBouncesPrev, VoxelSampler);

	float3 bounceColor = baseColor * traceSample.rgb * 0.5f;
	float shadow = diffuse.w;
	bounceColor += baseColor * shadow * 0.5f;

	float3 currentLightBounce = bounceColor;// + baseColor * saturate(0.5 - traceSample.w);

	float3 camOffset = (CameraPosition - VoxelsOffset) / VoxelsSceneSize;
	float3 diffOffset = abs(camOffset - voxelUV) * 2;
	float3 distanceFade = saturate(-4 * (diffOffset - 0.5) + 1);
	float fade = min(min(distanceFade.x, distanceFade.y), distanceFade.z);

	SceneVoxelBounces[id] = float4(currentLightBounce, 1) * fade;
}
