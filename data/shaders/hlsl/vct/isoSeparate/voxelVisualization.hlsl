#include "VoxelRayTracingCommon.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float4x4 PreviousWorldMatrix;
float3 CameraPosition;
uint2 ViewportSize;
uint VoxelIndex;
uint VoxelMip;

cbuffer SceneVoxelInfo : register(b1)
{
	SceneVoxelCbufferIndexed VoxelInfo;
};

struct VS_Input
{
	float4 p : POSITION;
	float2 uv : TEXCOORD0;
};

struct PS_Input
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 wp : TEXCOORD1;
};

PS_Input VSMain(VS_Input vin)
{
	PS_Input vsOut;

	vsOut.wp = mul(vin.p, WorldMatrix);
	vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
	vsOut.uv = vin.uv;

	return vsOut;
}

struct PSOutput
{
	float4 color : SV_Target0;
};

SamplerState PointSampler : register(s0);

PSOutput PSMain(PS_Input pin)
{
	float3 rayStart = pin.wp.xyz;
	float3 rayDir = normalize(pin.wp.xyz - CameraPosition);

	float4 result = RayTraceCascades(rayStart, rayDir, VoxelMip, VoxelInfo);

	if (!result.w)
		discard;

	PSOutput output;
	output.color = result;

	return output;
}