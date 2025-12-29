#include "VoxelConeTracingCommon.hlsl"

float3 CameraPosition;
float3 VoxelsOffset;
float3 VoxelsSceneSize;

StructuredBuffer<VoxelSceneData> VoxelData : register(t0);
Texture3D<float4> SceneVoxelBounces : register(t1);

RWTexture3D<float4> VoxelTexture : register(u0);
 
SamplerState VoxelSampler : register(s0);

[numthreads(4,4,4)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint linearIndex = id.z * (128 * 128) + id.y * 128 + id.x;

	float occupied = VoxelData[linearIndex].Occupy;

	if (WaveActiveAllTrue(occupied == 0))
		return;
	if (occupied == 0)
		return;

	float4 baseColor = VoxelData[linearIndex].Diffuse;
	float3 worldNormal = VoxelData[linearIndex].Normal;
	float3 voxelUV = (float3(id) + 0.5f) / 128;

	float4 fullTraceSample = ConeTrace(voxelUV, worldNormal, 1.05f, 1.5f, SceneVoxelBounces, VoxelSampler);

	float3 bounceColor = baseColor.rgb * fullTraceSample.rgb * 0.5f;
	float shadow = VoxelData[linearIndex].Max / 100.f;
	bounceColor += baseColor.rgb * shadow * 0.5f;

	//float3 prevBounceColor = SceneVoxelBounces[id].rgb;
	float3 currentLightBounce = bounceColor;

	float3 camOffset = (CameraPosition - VoxelsOffset) / VoxelsSceneSize;
	float3 diffOffset = abs(camOffset - voxelUV) * 2;
	float3 distanceFade = saturate(-4 * (diffOffset - 0.5) + 1);
	float fade = min(min(distanceFade.x, distanceFade.y), distanceFade.z);

	VoxelTexture[id] = float4(currentLightBounce, occupied) * saturate(fade);
}
