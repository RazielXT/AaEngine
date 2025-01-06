#include "VoxelConeTracingCommon.hlsl"

StructuredBuffer<VoxelSceneData> VoxelData : register(t0);
Texture3D<float4> SceneVoxelBounces : register(t1);

RWTexture3D<float4> VoxelTexture : register(u0);
 
SamplerState VoxelSampler : register(s0);

[numthreads(4,4,4)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint linearIndex = id.z * (128 * 128) + id.y * 128 + id.x;
	float3 worldNormal = VoxelData[linearIndex].Normal;

	float3 voxelUV = (float3(id) + 0.5f) / 128;
	float4 fullTraceSample = ConeTrace(voxelUV, worldNormal, 1.05f, 1.5f, SceneVoxelBounces, VoxelSampler, 0) * 3;

	float4 baseColor = VoxelData[linearIndex].Diffuse;
	float3 bounceColor = baseColor.rgb * fullTraceSample.rgb * 0.075f;
	bounceColor += baseColor.rgb * baseColor.w;

	float3 prevBounceColor = SceneVoxelBounces[id].rgb;
    VoxelTexture[id].rgb = lerp(prevBounceColor, bounceColor, 0.1);
}
