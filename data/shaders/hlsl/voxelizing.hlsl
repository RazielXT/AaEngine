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
	
//	int2 texCoord = int2(shadowCoord * 512);
//	return shadowmap.Load(int3(texCoord, 0)).r;
}

float getShadow(float4 wp)
{
	Texture2D shadowmap = GetTexture(Sun.TexIdShadowOffset + 1);
    float4 sunLookPos = mul(wp, Sun.ShadowMatrix[1]);
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

	float3 normal = normalize(mul(pin.normal, worldMatrix).xyz);
	float3 binormal = cross(pin.normal, pin.tangent);

	float3 diffuse = MaterialColor * GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, pin.uv).rgb;

    float3 geometryNormal = normal;
    float3 geometryB = normalize(mul(binormal, worldMatrix).xyz);
    float3 geometryT = normalize(mul(pin.tangent, worldMatrix).xyz);

	float3 voxelUV = (pin.wp.xyz - VoxelInfo.Voxels[VoxelIdx].Offset) / VoxelInfo.Voxels[VoxelIdx].SceneSize;
	Texture3D SceneVoxelBounces = GetTexture3D(VoxelInfo.Voxels[VoxelIdx].TexIdBounces);
	float4 fullTraceSample = ConeTrace(voxelUV, geometryNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, SceneVoxelBounces, VoxelSampler, 0) * 1;
    fullTraceSample += ConeTrace(voxelUV, normalize(geometryNormal + geometryT), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, SceneVoxelBounces, VoxelSampler, 0) * 1.0;
    fullTraceSample += ConeTrace(voxelUV, normalize(geometryNormal - geometryT), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, SceneVoxelBounces, VoxelSampler, 0) * 1.0;
    fullTraceSample += ConeTrace(voxelUV, normalize(geometryNormal + geometryB), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, SceneVoxelBounces, VoxelSampler, 0) * 1.0;
	fullTraceSample += ConeTrace(voxelUV, normalize(geometryNormal - geometryB), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, SceneVoxelBounces, VoxelSampler, 0) * 1.0;
    float3 traceColor = fullTraceSample.rgb;

	float3 baseColor = diffuse * traceColor * VoxelInfo.SteppingBounces; // + diffuse * VoxelInfo.Voxels[VoxelIdx].SteppingDiffuse;

    float occlusionSample = SampleVoxel(SceneVoxelBounces, VoxelSampler, voxelUV + geometryNormal / 128, geometryNormal, 2).w;
    occlusionSample += SampleVoxel(SceneVoxelBounces, VoxelSampler, voxelUV + normalize(geometryNormal + geometryT) / 128, normalize(geometryNormal + geometryT), 2).w;
    occlusionSample += SampleVoxel(SceneVoxelBounces, VoxelSampler, voxelUV + normalize(geometryNormal - geometryT) / 128, normalize(geometryNormal - geometryT), 2).w;
    occlusionSample += SampleVoxel(SceneVoxelBounces, VoxelSampler, voxelUV + (geometryNormal + geometryB) / 128, (geometryNormal + geometryB), 2).w;
    occlusionSample += SampleVoxel(SceneVoxelBounces, VoxelSampler, voxelUV + (geometryNormal - geometryB) / 128, (geometryNormal - geometryB), 2).w;
    occlusionSample = 1 - saturate(occlusionSample / 5);
	baseColor *= occlusionSample;
	
	if (dot(Sun.Direction,normal) < 0)
		baseColor += diffuse * getShadow(pin.wp) * 0.5;

    float3 posUV = (pin.wp.xyz - VoxelInfo.Voxels[VoxelIdx].Offset) * VoxelInfo.Voxels[VoxelIdx].Density;
	
	float3 prev = SceneVoxelBounces.Load(float4(posUV - VoxelInfo.Voxels[VoxelIdx].BouncesOffset * 32, 0)).rgb;
	baseColor.rgb = lerp(prev, baseColor.rgb, 0.1);

	baseColor = lerp(baseColor, diffuse * Emission, saturate(Emission));

	RWTexture3D<float4> SceneVoxel = ResourceDescriptorHeap[VoxelInfo.Voxels[VoxelIdx].TexId];
    SceneVoxel[posUV] = float4(baseColor, (1 - saturate(Emission)));
	//SceneVoxel[posUV - normal].w = (1 - saturate(Emission));

	return float4(diffuse, 1);
}
