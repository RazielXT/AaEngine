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

float getDistanceBlend(float dist, float sceneSize)
{
	sceneSize *= 0.5;
	float maxDistance = sceneSize * 0.75;

	const float fadeStart = maxDistance * 0.8;
	const float fadeRange = maxDistance * 0.2;

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
	float camDistance = length(CameraPosition - pin.wp.xyz);

	float3x3 worldMatrix = (float3x3)WorldMatrix;
	float3 worldNormal = normalize(mul(pin.normal, worldMatrix));
	float3 worldTangent = normalize(mul(pin.tangent, worldMatrix));
	float3 worldBinormal = cross(worldNormal, worldTangent);

	float3 voxelAmbient = 0;
	//far cascade
//	{
		float3 voxelUVFar = (pin.wp.xyz - VoxelInfo.FarVoxels.Offset) / VoxelInfo.FarVoxels.SceneSize;
		Texture3D voxelmapFar = GetTexture3D(VoxelInfo.FarVoxels.TexId);
		float4 fullTraceFar = ConeTrace(voxelUVFar, worldNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, voxelmapFar, VoxelSampler, 0);
		{
			float occlusionSample = SampleVoxel(voxelmapFar, VoxelSampler, voxelUVFar + worldNormal / 128, worldNormal, 2).w;
			occlusionSample += SampleVoxel(voxelmapFar, VoxelSampler, voxelUVFar + normalize(worldNormal + worldTangent) / 128, normalize(worldNormal + worldTangent), 2).w;
			occlusionSample += SampleVoxel(voxelmapFar, VoxelSampler, voxelUVFar + normalize(worldNormal - worldTangent) / 128, normalize(worldNormal - worldTangent), 2).w;
			occlusionSample += SampleVoxel(voxelmapFar, VoxelSampler, voxelUVFar + (worldNormal + worldBinormal) / 128, (worldNormal + worldBinormal), 2).w;
			occlusionSample += SampleVoxel(voxelmapFar, VoxelSampler, voxelUVFar + (worldNormal - worldBinormal) / 128, (worldNormal - worldBinormal), 2).w;
			occlusionSample = 1 - saturate(occlusionSample / 5);
			fullTraceFar *= occlusionSample;
		}

		voxelAmbient = fullTraceFar.rgb;
//	}

#ifndef GI_LOW
	//near cascade
	float nearFarBlend = getDistanceBlend(camDistance, VoxelInfo.NearVoxels.SceneSize);
//	{
		float3 voxelUV = (pin.wp.xyz - VoxelInfo.NearVoxels.Offset) / VoxelInfo.NearVoxels.SceneSize;
		Texture3D voxelmap = GetTexture3D(VoxelInfo.NearVoxels.TexId);
		float4 fullTraceNear = ConeTrace(voxelUV, worldNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, voxelmap, VoxelSampler, 0);
#ifndef GI_MEDIUM
		fullTraceNear += ConeTrace(voxelUV, normalize(worldNormal + worldTangent), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0);
		fullTraceNear += ConeTrace(voxelUV, normalize(worldNormal - worldTangent), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0);
		fullTraceNear += ConeTrace(voxelUV, normalize(worldNormal + worldBinormal), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0);
		fullTraceNear += ConeTrace(voxelUV, normalize(worldNormal - worldBinormal), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0);
		fullTraceNear.rgb /= 5;
#endif //GI_MEDIUM
		{
			float occlusionSample = SampleVoxel(voxelmap, VoxelSampler, voxelUV + worldNormal / 128, worldNormal, 2).w;
			occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + normalize(worldNormal + worldTangent) / 128, normalize(worldNormal + worldTangent), 2).w;
			occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + normalize(worldNormal - worldTangent) / 128, normalize(worldNormal - worldTangent), 2).w;
			occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + (worldNormal + worldBinormal) / 128, (worldNormal + worldBinormal), 2).w;
			occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + (worldNormal - worldBinormal) / 128, (worldNormal - worldBinormal), 2).w;
			occlusionSample = 1 - saturate(occlusionSample / 5);	
			fullTraceNear *= occlusionSample;
		}
		
		voxelAmbient = lerp(fullTraceNear.rgb, voxelAmbient, nearFarBlend);
//	}
#endif //GI_LOW

	float3 staticAmbient = float3(0.2, 0.2, 0.2);

	float voxelWeight = getDistanceBlend(camDistance, VoxelInfo.FarVoxels.SceneSize);
	float3 finalAmbient = lerp(voxelAmbient, staticAmbient, voxelWeight);
	
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];
    float3 albedo = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, pin.uv / 20).rgb;
	albedo *= MaterialColor;

	float directShadow = getShadow(pin.wp);
	float directLight = saturate(dot(-Sun.Direction,worldNormal)) * directShadow;
	float3 lighting = max(directLight, finalAmbient);

    float4 color1 = float4(saturate(albedo * lighting), 1);
	color1.rgb += albedo * Emission;
	color1.a = (lighting.r + lighting.g + lighting.b) / 3;
	
	float3 nearVoxel = voxelmap.SampleLevel(VoxelSampler, voxelUV, 0).rgb;
	float3 farVoxel = voxelmapFar.SampleLevel(VoxelSampler, voxelUVFar, 0).rgb;
	//color1.rgb = farVoxel;
	//color1.rgb = lerp(nearVoxel, farVoxel, nearFarBlend);

	PSOutput output;
    output.color = color1;
	output.normals = float4(worldNormal, 1);

	output.camDistance = float4(camDistance / 10000, 0, 0, 0);

	output.motionVectors = float4((pin.previousPosition.xy / pin.previousPosition.w - pin.currentPosition.xy / pin.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;

	return output;
}
