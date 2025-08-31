#include "VoxelConeTracingCommon.hlsl"
#include "ShadowsPssm.hlsl"

#ifdef BRANCH_WAVE
#include "TreeCommon.hlsl"
#endif

float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float4x4 PreviousWorldMatrix;
float3 CameraPosition;
float Emission;
float3 MaterialColor;
uint TexIdDiffuse;
uint TexIdNormal;
uint2 ViewportSize;
float Time;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

cbuffer SceneVoxelInfo : register(b2)
{
    SceneVoxelCbufferIndexed VoxelInfo;
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
	
#ifdef BRANCH_WAVE
	vsOut.wp.xyz += getBranchWaveOffset(Time, vin.p.xyz, vin.uv);
#endif

    vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
    vsOut.uv = vin.uv;
    vsOut.normal = vin.n;
    vsOut.tangent = vin.t;
	
	float4 previousWorldPosition = mul(vin.p, PreviousWorldMatrix);
#ifdef BRANCH_WAVE
	previousWorldPosition.xyz += getBranchWaveOffset(Time, vin.p.xyz, vin.uv);
#endif
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

#ifdef TERRAIN_SCAN

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 types : SV_Target2;
    float4 height : SV_Target3;
};

PSOutput PSMain(PS_Input pin)
{
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, pin.uv);

	PSOutput output;
    output.color = albedo;
	output.normals = float4(pin.normal, 1);
	output.types = float4(1, 1, 0, 0);
	output.height = float4(pin.wp.y, 0, 0, 0);

	return output;
}

#else

SamplerState ShadowSampler : register(s1);

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
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];

    float4 albedo = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, pin.uv);
#ifdef ALPHA_TEST
	if (albedo.a < 0.37f)
		discard;
#endif
	albedo.rgb *= MaterialColor;

	float camDistance = length(CameraPosition - pin.wp.xyz);

    float3 normalTex = float3(GetTexture(TexIdNormal).Sample(diffuse_sampler, pin.uv).rg, 1);
    //normalTex = (normalTex + float3(0.5f, 0.5f, 1.0f) * 3) / 4;
	float3 bin = cross(pin.normal, pin.tangent);
    float3x3 tbn = float3x3(pin.tangent, bin, pin.normal);
    float3 normal = mul(normalTex.xyz * 2 - 1, tbn); // to object space

	float3x3 worldMatrix = (float3x3)WorldMatrix;
	float3 worldDetailNormal = normalize(mul(normal, worldMatrix));

	float3 worldNormal = normalize(mul(normal, worldMatrix));
	float3 worldTangent = normalize(mul(pin.tangent, worldMatrix));
	float3 worldBinormal = cross(worldNormal, worldTangent);

	float3 voxelAmbient = 0;
	float globalOcclusion = 1;
	float voxelWeight = 1.0f;
	float fullWeight = 0.01f;

	for (int idx = 0; idx < 4; idx++)
	{
		float3 voxelUV = (pin.wp.xyz - VoxelInfo.Voxels[idx].Offset) / VoxelInfo.Voxels[idx].SceneSize;
		Texture3D voxelmap = GetTexture3D(VoxelInfo.Voxels[idx].TexId);
		
		float mipOffset = idx == 3 ? 0 : -1;
		float4 fullTrace = ConeTraceImpl(voxelUV, worldNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, voxelmap, VoxelSampler, mipOffset);

		float offsetScale = idx >= 2 ? 128 : 64;
		float mipTarget = idx >= 2 ? 1 : 2;
		float occlusionSample = SampleVoxel(voxelmap, VoxelSampler, voxelUV + worldNormal / offsetScale, worldNormal, mipTarget).w;
		occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + (worldNormal + worldTangent) / offsetScale, (worldNormal + worldTangent), mipTarget).w;
		occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + (worldNormal - worldTangent) / offsetScale, (worldNormal - worldTangent), mipTarget).w;
		occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + (worldNormal + worldBinormal) / offsetScale, (worldNormal + worldBinormal), mipTarget).w;
		occlusionSample += SampleVoxel(voxelmap, VoxelSampler, voxelUV + (worldNormal - worldBinormal) / offsetScale, (worldNormal - worldBinormal), mipTarget).w;
		occlusionSample = 1 - saturate(occlusionSample / 5);
		globalOcclusion = min(globalOcclusion, fullTrace.w * occlusionSample);

		voxelAmbient += fullTrace.rgb * voxelWeight;

		//if (idx >= 2)
		{
			voxelWeight = saturate(voxelWeight - fullTrace.w);
			fullWeight += fullTrace.w;
		}
	}

	voxelAmbient /= max(0.5f, pow(fullWeight, 1));
	fullWeight /= 4.0f;
	fullWeight = 1 - fullWeight;

	float dotStepLighting = step(0, dot(-Sun.Direction,worldNormal));
	float dotLighting = dot(-Sun.Direction,worldDetailNormal);
	float3 skyAmbient = float3(0.3, 0.3, 0.35) * (0.15 + 0.5 * saturate(worldDetailNormal.y));

	float voxelAmbientWeight = getDistanceBlend(camDistance, VoxelInfo.Voxels[3].SceneSize);
	float3 finalAmbient = lerp(voxelAmbient, skyAmbient, voxelAmbientWeight);

	float3 skyLighting = fullWeight * skyAmbient;
	finalAmbient = saturate(finalAmbient + skyLighting);

	float directShadow = getPssmShadow(pin.wp, camDistance, dotLighting, ShadowSampler, Sun) * dotStepLighting;
	float directLight = saturate(dotLighting) * directShadow;
	float3 lighting = (directLight * Sun.Color + finalAmbient);

    float4 color1 = float4(saturate(albedo.rgb * lighting), 1);
	color1.rgb += albedo.rgb * Emission;
	color1.a = (lighting.r + lighting.g + lighting.b) / 30;

	//color1.rgb = finalAmbient;
	//color1.rgb = pin.normal;

	PSOutput output;
    output.color = color1;
	output.normals = float4(worldDetailNormal, 1);

	output.camDistance = float4(camDistance, 0, 0, 0);

	output.motionVectors = float4((pin.previousPosition.xy / pin.previousPosition.w - pin.currentPosition.xy / pin.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;

	return output;
}

#endif
