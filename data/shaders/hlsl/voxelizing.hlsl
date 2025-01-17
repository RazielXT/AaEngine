#include "VoxelConeTracingCommon.hlsl"
#include "ShadowsCommon.hlsl"

#ifdef INSTANCED
float4x4 ViewProjectionMatrix;
#else
float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
#endif

float3 MaterialColor;
uint TexIdDiffuse;

float Emission;
uint VoxelIdx;

cbuffer SceneVoxelInfo : register(b1)
{
    SceneVoxelCbufferIndexed VoxelInfo;
};

cbuffer PSSMShadows : register(b2)
{
	SunParams Sun;
}

#ifdef INSTANCED
StructuredBuffer<float4x4> InstancingBuffer : register(t0);
#endif

RWStructuredBuffer<VoxelSceneData> SceneVoxelData : register(u0);

struct VS_Input
{
    float4 p : POSITION;
    float2 uv : TEXCOORD0;
    float3 n : NORMAL;
    float3 t : TANGENT;

#ifdef INSTANCED
	uint instanceID : SV_InstanceID;
#endif
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 wp : TEXCOORD1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
	
#ifdef INSTANCED
	uint instanceID : TEXCOORD2;
#endif
};

PS_Input VS_Main(VS_Input vin)
{
    PS_Input vsOut;

    vsOut.uv = vin.uv;
    vsOut.tangent = vin.t;
	vsOut.normal = vin.n;

#ifdef INSTANCED
	vsOut.wp = mul(vin.p, InstancingBuffer[vin.instanceID]);
	vsOut.instanceID = vin.instanceID;
#else
	vsOut.wp = mul(vin.p, WorldMatrix);
#endif

    vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);

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

SamplerState ShadowSampler : register(s2);

float readShadowmap(Texture2D shadowmap, float2 shadowCoord)
{
	return shadowmap.SampleLevel(ShadowSampler, shadowCoord, 0).r;
}

float getShadow(float4 wp)
{
	Texture2D shadowmap = GetTexture(Sun.TexIdShadowOffset + 3);
    float4 sunLookPos = mul(wp, Sun.MaxShadowMatrix);
    sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;
	sunLookPos.z -= 0.01;

	return readShadowmap(shadowmap, sunLookPos.xy) < sunLookPos.z ? 0.0 : 1.0;
}

SamplerState LinearWrapSampler : register(s0);
SamplerState VoxelSampler : register(s1);

float4 PS_Main(PS_Input pin) : SV_TARGET
{
#ifdef INSTANCED
	float3x3 worldMatrix = (float3x3)InstancingBuffer[pin.instanceID];
#else
	float3x3 worldMatrix = (float3x3)WorldMatrix;
#endif

	float3 worldNormal = normalize(mul(pin.normal, worldMatrix));

	float3 diffuse = MaterialColor * GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, pin.uv).rgb;

	Texture3D SceneVoxelBounces = GetTexture3D(VoxelInfo.Voxels[VoxelIdx].TexIdBounces);

	float shadow = 0;
	if (dot(Sun.Direction,worldNormal) < 0)
		shadow = getShadow(pin.wp);

	float3 voxelWorldPos = (pin.wp.xyz - VoxelInfo.Voxels[VoxelIdx].Offset);
    float3 posUV = voxelWorldPos * VoxelInfo.Voxels[VoxelIdx].Density;

	float4 prev = SceneVoxelBounces.Load(float4(posUV - VoxelInfo.Voxels[VoxelIdx].BouncesOffset * 32, 0));

	RWTexture3D<float4> SceneVoxel = ResourceDescriptorHeap[VoxelInfo.Voxels[VoxelIdx].TexId];
    SceneVoxel[posUV] = float4(prev.rgb * prev.w, 1);
	//SceneVoxel[posUV - worldNormal].a = 1;

	bool isInBounds = (posUV.x >= 0 && posUV.x < 128) &&
					  (posUV.y >= 0 && posUV.y < 128) &&
					  (posUV.z >= 0 && posUV.z < 128);

	if (isInBounds)
	{
		uint linearIndex = uint(posUV.z) * 128 * 128 + uint(posUV.y) * 128 + uint(posUV.x);

		//RWStructuredBuffer<VoxelSceneData> SceneVoxelData = ResourceDescriptorHeap[NonUniformResourceIndex(VoxelInfo.Voxels[VoxelIdx].ResIdData)];

		int newCheckValue = shadow * 100;
		
		int previousCheckValue = 0;
		InterlockedMax(SceneVoxelData[linearIndex].Max, newCheckValue, previousCheckValue);

		if (previousCheckValue <= newCheckValue)
		{
			SceneVoxelData[linearIndex].Diffuse = float4(diffuse, shadow);
			SceneVoxelData[linearIndex].Normal = worldNormal;
		}
	}

	return float4(diffuse * shadow, 1);
}
