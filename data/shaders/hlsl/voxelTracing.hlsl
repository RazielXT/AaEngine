#include "VoxelConeTracingCommon.hlsl"
#include "ShadowsCommon.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float4x4 PreviousWorldMatrix;
float3 CameraPosition;
float Emission;
float3 MaterialColor;
uint TexIdDiffuse;
uint2 ViewportSize;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

cbuffer SceneVoxelInfo : register(b2)
{
    SceneVoxelCbuffer VoxelInfo;
};

struct VS_Input
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float3 t : TANGENT;
    float2 uv : TEXCOORD0;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 wp : TEXCOORD1;
	float4 currentPosition : TEXCOORD2;
	float4 previousPosition : TEXCOORD3;
};

PS_Input VSMain(VS_Input vin)
{
    PS_Input vsOut;

    vsOut.wp = mul(vin.p, WorldMatrix);
    vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
    vsOut.uv = vin.uv;
    vsOut.normal = vin.n;
    vsOut.tangent = vin.t;
	
	float4 previousWorldPosition = mul(vin.p, PreviousWorldMatrix);
	vsOut.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	vsOut.currentPosition = vsOut.pos;

    return vsOut;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
Texture3D<float4> GetTexture3D(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState ShadowSampler : register(s1);

float getShadow(float4 wp)
{
	Texture2D shadowmap = GetTexture(Sun.TexIdShadowOffset);
    float4 sunLookPos = mul(wp, Sun.ShadowMatrix[0]);
    sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;
	sunLookPos.z -= 0.005;

	return CalcShadowTermSoftPCF(shadowmap, ShadowSampler, sunLookPos.z, sunLookPos.xy, 5, Sun.ShadowMapSize, Sun.ShadowMapSizeInv);
}

float getDistanceBlend(float dist, float maxDistance)
{
	const float fadeStart = maxDistance * 0.7;
	const float fadeRange = maxDistance * 0.3;

	return saturate((dist - fadeStart) / fadeRange); 
}

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

SamplerState VoxelSampler : register(s0);

PSOutput PSMain(PS_Input pin)
{
	float3 binormal = cross(pin.normal, pin.tangent);
	float3 normal = normalize(mul(pin.normal, (float3x3) WorldMatrix).xyz);

    float3 geometryNormal = normal;
    float3 geometryB = normalize(mul(binormal, (float3x3) WorldMatrix).xyz);
    float3 geometryT = normalize(mul(pin.tangent, (float3x3) WorldMatrix).xyz);

    float3 voxelUV = (pin.wp.xyz - VoxelInfo.NearVoxels.Offset) / VoxelInfo.NearVoxels.SceneSize;
	Texture3D voxelmap = GetTexture3D(VoxelInfo.NearVoxels.TexId);
    float4 fullTraceSample = ConeTrace(voxelUV, geometryNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, voxelmap, VoxelSampler, 0) * 1;
    fullTraceSample += ConeTrace(voxelUV, normalize(geometryNormal + geometryT), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0) * 1.0;
    fullTraceSample += ConeTrace(voxelUV, normalize(geometryNormal - geometryT), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0) * 1.0;
    fullTraceSample += ConeTrace(voxelUV, normalize(geometryNormal + geometryB), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0) * 1.0;
    fullTraceSample += ConeTrace(voxelUV, normalize(geometryNormal - geometryB), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0) * 1.0;
    float3 traceColor = fullTraceSample.rgb / 5;

    float occlusionSample = SampleVoxel(voxelmap, VoxelSampler, voxelUV + geometryNormal / 128, geometryNormal, 2).w;
    occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + normalize(geometryNormal + geometryT) / 128, normalize(geometryNormal + geometryT), 2).w;
    occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + normalize(geometryNormal - geometryT) / 128, normalize(geometryNormal - geometryT), 2).w;
    occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + (geometryNormal + geometryB) / 128, (geometryNormal + geometryB), 2).w;
    occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + (geometryNormal - geometryB) / 128, (geometryNormal - geometryB), 2).w;
    occlusionSample = 1 - saturate(occlusionSample / 5);

	traceColor *= occlusionSample;

	float3 voxelUVFar = (pin.wp.xyz - VoxelInfo.FarVoxels.Offset) / VoxelInfo.FarVoxels.SceneSize;
	Texture3D voxelmapFar = GetTexture3D(VoxelInfo.FarVoxels.TexId);
	float4 fullTraceSampleFar = ConeTrace(voxelUVFar, geometryNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, voxelmapFar, VoxelSampler, 0);

	float camDistance = length(CameraPosition - pin.wp.xyz);

	float3 voxelAmbient = lerp(traceColor, fullTraceSampleFar.rgb, getDistanceBlend(camDistance, VoxelInfo.NearVoxels.SceneSize * 0.5));
	float3 staticAmbient = float3(0.2, 0.2, 0.2);

	float voxelWeight = getDistanceBlend(camDistance, VoxelInfo.FarVoxels.SceneSize * 0.5);
	float3 finalAmbient = lerp(voxelAmbient, staticAmbient, voxelWeight);
	
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];
    float3 albedo = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, pin.uv / 20).rgb;
	albedo *= MaterialColor;

	float directShadow = getShadow(pin.wp);
	float directLight = saturate(dot(-Sun.Direction,normal)) * directShadow;
	float3 lighting = max(directLight, finalAmbient);

    float4 color1 = float4(saturate(albedo * lighting), 1);
	color1.rgb += albedo * Emission;
	color1.a = (lighting.r + lighting.g + lighting.b) / 3;
	//color1.rgb = voxelmap.SampleLevel(VoxelSampler, voxelUV, 0).rgb;

	PSOutput output;
    output.color = color1;
	output.normals = float4(normal, 1);

	output.camDistance = float4(camDistance / 10000, 0, 0, 0);

	output.motionVectors = float4((pin.previousPosition.xy / pin.previousPosition.w - pin.currentPosition.xy / pin.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;

	return output;
}
