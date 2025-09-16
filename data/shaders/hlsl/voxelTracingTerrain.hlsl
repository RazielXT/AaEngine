#include "VoxelConeTracingCommon.hlsl"
#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float3 MainCameraPosition;
float BlendDistance;
uint2 ViewportSize;
uint TexIdDiffuse;
uint TexIdGrass;
uint TexIdNormal;
uint TexIdGrassNormal;
uint TexIdSpread;
uint TexIdDisplacement;

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
    float4 nhLod : TEXCOORD1;
    float3 tLod : TEXCOORD2;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 wp : TEXCOORD1;
	float www : TEXCOORD4;
};

float getDistanceBlend(float dist, float sceneSize)
{
	sceneSize *= 0.5f;
	float maxDistance = sceneSize * 0.75f;

	const float fadeStart = maxDistance * 0.8f;
	const float fadeRange = maxDistance * 0.2f;

	return saturate((dist - fadeStart) / fadeRange); 
}

float getDistanceBlend2(float dist, float maxDistance)
{
	const float fadeStart = maxDistance * 0.8f;
	const float fadeRange = maxDistance * 0.2f;

	return saturate((dist - fadeStart) / fadeRange); 
}

PS_Input VSMain(VS_Input vin)
{
    PS_Input vsOut;

    vsOut.wp = mul(vin.p, WorldMatrix);

	float camDistance = length(MainCameraPosition.xz - vsOut.wp.xz);
	float distanceLerp = getDistanceBlend2(camDistance, BlendDistance);
	vsOut.wp.y -= vin.nhLod.w * distanceLerp;
	vsOut.www = distanceLerp;
    vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
    vsOut.uv = vsOut.wp.xz / 25.f;
    vsOut.normal = lerp(vin.n, vin.nhLod.xyz, distanceLerp);
    vsOut.tangent = lerp(vin.t, vin.tLod.xyz, distanceLerp);

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

SamplerState VoxelSampler : register(s0);
SamplerState ShadowSampler : register(s1);

float3 CreateDistanceWeigths3(float camDistance, float distances[3], float scales[3])
{
	float lerpWeight = 1;
	float closeDetailUv = 1;
	float farDetailUv = scales[0];

	for (int i = 0; i < 2; i++)
	{
		lerpWeight = saturate((camDistance - distances[i]) / (distances[i + 1] - distances[i]));
		closeDetailUv = farDetailUv;
		farDetailUv = scales[i + 1];
		
		if (lerpWeight != 1.f)
			break;
	}
	
	return float3(closeDetailUv, farDetailUv, lerpWeight);
}

float3 CreateDistanceWeigths5(float camDistance, float distances[5], float scales[5])
{
	float lerpWeight = 1;
	float closeDetailUv = 1;
	float farDetailUv = scales[0];

	for (int i = 0; i < 4; i++)
	{
		lerpWeight = saturate((camDistance - distances[i]) / (distances[i + 1] - distances[i]));
		closeDetailUv = farDetailUv;
		farDetailUv = scales[i + 1];
		
		if (lerpWeight != 1.f)
			break;
	}
	
	return float3(closeDetailUv, farDetailUv, lerpWeight);
}

float3 ReadDistanceTexture(uint texId, SamplerState sampler, float2 uv, float3 weights)
{
	float3 normalTexClose = GetTexture(texId).Sample(sampler, uv * weights.x).rgb;
	float3 normalTex = GetTexture(texId).Sample(sampler, uv * weights.y).rgb;

	return lerp(normalTexClose, normalTex, weights.z);
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
	
	float3 rock = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, pin.uv).rgb;
	float3 green = GetTexture(TexIdGrass).Sample(diffuse_sampler, pin.uv * 5).rgb * float3(0.9, 1, 0.9);
	float3 brown = float3(0.45, 0.35, 0.25) * 1.5f * rock; // Color for brown
	
	float spread = GetTexture(TexIdSpread).Sample(diffuse_sampler, pin.uv / 10).r;
	float detailSpread = GetTexture(TexIdSpread).Sample(diffuse_sampler, pin.uv * 2).r * spread * spread  * 0.1;
	float spread2 = GetTexture(TexIdSpread).Sample(diffuse_sampler, (pin.uv  + 0.5) / 3).r;
	float detailSpread2 = GetTexture(TexIdSpread).Sample(diffuse_sampler, (pin.uv  + 0.5) * 10).r * spread2 * spread2 * 0.1;
	
	float rockWeight = smoothstep(0.77 + detailSpread, 0.78 + detailSpread, pin.normal.y);
	float3 albedoMid = lerp(rock, brown, rockWeight);
	float grassWeight = smoothstep(0.78 + detailSpread2, 0.8 + detailSpread2, pin.normal.y);
	float3 albedo = lerp(albedoMid, green, grassWeight);
	
	PSOutput output;
    output.color = float4(albedo, 1);
	output.normals = float4(pin.normal, 1);
	output.types = float4(rockWeight, grassWeight, 0, 0);
	output.height = float4(pin.wp.y, 0, 0, 0);

	return output;
}

#else

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

PSOutput PSMain(PS_Input pin)
{
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];
	float3 cameraView = MainCameraPosition - pin.wp.xyz;
	float camDistance = length(cameraView);

	float3 inNormal = pin.normal;
	//inNormal.y /= 2;
	//inNormal = normalize(inNormal);
	float3 bin = cross(inNormal, pin.tangent);

	//pm offset
	//float height = GetTexture(TexIdDisplacement).Sample(diffuse_sampler, pin.uv * 4).r;
	//float3 vVec = float3(dot(cameraView, pin.tangent.xyz), dot(cameraView, bin.xyz), dot(cameraView, pin.normal.xyz));
	//float dispWeight = lerp(-0.01, 0, saturate(camDistance / 500));
	//float2 texCoordDispOffset = dispWeight * (height - 0.5) * normalize(vVec).xy;

	float spread = GetTexture(TexIdSpread).Sample(diffuse_sampler, pin.uv / 10).r;
	float detailSpread = GetTexture(TexIdSpread).Sample(diffuse_sampler, pin.uv * 2).r * spread * spread  * 0.1;
	float spread2 = GetTexture(TexIdSpread).Sample(diffuse_sampler, (pin.uv  + 0.5) / 3).r;
	float detailSpread2 = GetTexture(TexIdSpread).Sample(diffuse_sampler, (pin.uv  + 0.5) * 10).r * spread2 * spread2 * 0.1;

	float rockWeight = smoothstep(0.77 + detailSpread, 0.78 + detailSpread, pin.normal.y);
	float grassWeight = smoothstep(0.78 + detailSpread2, 0.8 + detailSpread2, pin.normal.y);

	//texCoordDispOffset *= 1 - grassWeight;
	float2 texCoordWithOffset = pin.uv;

	float diffuseStepDistance[5] = { 0, 50, 500, 2000, 6000 };
	float diffuseStepScale[5] = { 4, 2, 1.f /2, 1.f /5, 1.f /20 };
	float3 diffuseDistanceWeights = CreateDistanceWeigths5(camDistance, diffuseStepDistance, diffuseStepScale);
	float3 rock = ReadDistanceTexture(TexIdDiffuse, diffuse_sampler, texCoordWithOffset, diffuseDistanceWeights);

	float3 green = GetTexture(TexIdGrass).Sample(diffuse_sampler, pin.uv * 5).rgb * float3(0.9, 1, 0.9);
	float3 brown = float3(0.45, 0.35, 0.25) * 1.5f * rock; // Color for brown

	float3 albedoMid = lerp(rock, brown, rockWeight);
	float3 albedo = lerp(albedoMid, green, grassWeight);

	uint NormalTex = rockWeight > 0.5 ? TexIdGrassNormal : TexIdNormal;
	float3 normalTex = ReadDistanceTexture(NormalTex, diffuse_sampler, texCoordWithOffset, diffuseDistanceWeights);
	normalTex.b = 1;
	//normalTex = lerp(normalTex, float3(0.5f, 0.5f, 1.0f), 0.5);

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
		
		float mipOffset = idx > 2 ? 0 : -1;
		float4 fullTrace = ConeTraceImpl(voxelUV, worldNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, voxelmap, VoxelSampler, mipOffset);
		fullTrace.rgb *= 4;
		float traceW = fullTrace.w;
		fullTrace += ConeTraceImpl(voxelUV, normalize(worldNormal + worldTangent), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0) * 1.0;
		fullTrace += ConeTraceImpl(voxelUV, normalize(worldNormal - worldTangent), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0) * 1.0;
		fullTrace += ConeTraceImpl(voxelUV, normalize(worldNormal + worldBinormal), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0) * 1.0;
		fullTrace += ConeTraceImpl(voxelUV, normalize(worldNormal - worldBinormal), VoxelInfo.SideConeRatio.x, VoxelInfo.SideConeRatio.y, voxelmap, VoxelSampler, 0) * 1.0;
		fullTrace /= 5;
		traceW = min(traceW, fullTrace.w);

		if (idx < 3)
			fullTrace *= 1 - getDistanceBlend(camDistance, VoxelInfo.Voxels[idx].SceneSize);

		//if (idx >= 1)
		voxelAmbient += fullTrace.rgb * voxelWeight;

		if (idx >= 2)
		{
			voxelWeight = saturate(voxelWeight - traceW);
			fullWeight += traceW;
		}
	}

	//voxelWeight = pow(1 - voxelWeight, 2);
	//voxelAmbient /= max(0.9f, voxelWeight);
	fullWeight /= 4.0f;
	fullWeight = pow(1 - fullWeight, 1);

	float dotStepLighting = step(0, dot(-Sun.Direction,worldNormal));
	float dotLighting = dot(-Sun.Direction,worldDetailNormal);
	float3 skyAmbient = float3(0.2, 0.2, 0.3) * (0.15 + 0.5 * saturate(worldDetailNormal.y));

	float voxelFarWeight = getDistanceBlend(camDistance, VoxelInfo.Voxels[3].SceneSize);
	float3 finalAmbient = lerp(voxelAmbient, skyAmbient, voxelFarWeight);

	float3 skyLighting = fullWeight * skyAmbient;
	finalAmbient = saturate(finalAmbient + skyLighting);

	float directShadow = getPssmShadow(pin.wp, camDistance, dotLighting, ShadowSampler, Sun) * dotStepLighting;
	float directLight = abs(dotLighting) * directShadow;
	float3 lighting = (pow(directLight, 2) * Sun.Color + finalAmbient);

    float4 color1 = float4(albedo * lighting, 1);
	color1.a = (lighting.r + lighting.g + lighting.b) / 30;

	//color1.rgb = voxelAmbient;
	//color1.rgb = skyLighting;
	//color1.rgb = directShadow;
	//color1.rgb = color1.rgb * 0.01 + step(0.99, pin.www);
	
	PSOutput output;
    output.color = color1;
	output.normals = float4(worldDetailNormal, 1);

	output.camDistance = camDistance;

	output.motionVectors = float4(0, 0, 0, 0);

	return output;
}

#endif
