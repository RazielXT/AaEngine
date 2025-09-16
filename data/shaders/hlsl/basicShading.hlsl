#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 PreviousWorldMatrix;
float4x4 WorldMatrix;
float3 MaterialColor;
uint TexIdDiffuse;
float3 MainCameraPosition;
uint2 ViewportSize;
#ifdef VERTEX_WAVE
float Time;
float DeltaTime;
#endif

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

#ifdef INSTANCED
StructuredBuffer<float4x4> InstancingBuffer : register(t0);
#endif

struct VSInput
{
    float4 position : POSITION;
	float3 normal : NORMAL;
#ifndef NO_TEXTURE
    float2 uv : TEXCOORD;
#endif
#ifdef USE_VC
	float4 color : COLOR;
#endif
#ifdef INSTANCED
	uint instanceID : SV_InstanceID;
#endif
};

struct PSInput
{
    float4 position : SV_POSITION;
	float3 normal : NORMAL;
#ifdef USE_VC
    float4 color : COLOR;
#endif
#ifndef NO_TEXTURE
	float2 uv : TEXCOORD0;
#endif
	float4 worldPosition : TEXCOORD1;
	float4 previousPosition : TEXCOORD2;
	float4 currentPosition : TEXCOORD3;
};

float3 WindWave(float3 basePos, float swayFactor, float seed)
{
	const float3 WindDirection = float3(1,0.5,0);
	const float  WindStrength = 2.5f;
	const float  WindSpeed = 3;

	float leafRandom = (basePos.z + basePos.x)/5;
	float phase = WindSpeed * seed + leafRandom + swayFactor * 3;
	float windOffset = ((sin(phase*0.4 + 0.5) + sin(phase)) * 0.25 + 0.75) * WindStrength;

	float3 windDisplacement = WindDirection * windOffset * swayFactor;
	return basePos + windDisplacement;
}

void ComputeBaseBend(float4 basePosOS, inout float4 vertexPosOS, float w)
{
	vertexPosOS.y -= dot(basePosOS.xz, basePosOS.xz) * 0.15 * w;
}

PSInput VSMain(VSInput input)
{
	PSInput result;

#ifdef INSTANCED
	result.worldPosition = mul(input.position, InstancingBuffer[input.instanceID]);
	#ifdef VERTEX_WAVE
		float waveWeight = saturate(1 - length(MainCameraPosition - result.worldPosition.xyz)/400) * (1-input.uv.y);
		ComputeBaseBend(input.position, result.worldPosition, waveWeight);
		float4 worldPositionNoWave = result.worldPosition;
		result.worldPosition.xyz = WindWave(result.worldPosition.xyz, waveWeight, Time);
	#endif
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = normalize(mul(input.normal, (float3x3)InstancingBuffer[input.instanceID]));
	
	#ifdef VERTEX_WAVE
		float4 previousWorldPosition = float4(WindWave(worldPositionNoWave.xyz, waveWeight, Time - DeltaTime), 1);
		result.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	#else
		result.previousPosition = result.position; // no support
	#endif
	result.currentPosition = result.position;
#else
	result.worldPosition = mul(input.position, WorldMatrix);
	#ifdef VERTEX_WAVE
		result.worldPosition.xyz = WindWave(result.worldPosition.xyz, 1-input.uv.y, Time);
	#endif
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = normalize(mul(input.normal, (float3x3)WorldMatrix));

	float4 previousWorldPosition = mul(input.position, PreviousWorldMatrix);
	#ifdef VERTEX_WAVE
		previousWorldPosition.xyz = WindWave(previousWorldPosition.xyz, 1-input.uv.y, Time - DeltaTime);
	#endif
	result.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	result.currentPosition = result.position;
#endif

#ifdef USE_VC
    result.color = input.color;
#endif

#ifndef NO_TEXTURE
	result.uv = input.uv;
	#ifndef VERTEX_WAVE
		result.uv *= 10;
	#endif
#endif

    return result;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState ShadowSampler : register(s0);

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

PSOutput PSMain(PSInput input)
{
	float3 normal = input.normal;//normalize(mul(input.normal, (float3x3)WorldMatrix).xyz);

	float dotLighting = dot(-Sun.Direction, normal);
	float3 diffuse = saturate(dotLighting) * Sun.Color;

#ifdef USE_VC
	float3 ambientColor = input.color.rgb;
#else
	float3 ambientColor = float3(0.2,0.2,0.2);
#endif

	SamplerState sampler = SamplerDescriptorHeap[0];

#ifndef NO_TEXTURE
	float4 albedoTex = GetTexture(TexIdDiffuse).Sample(sampler, input.uv);
	#ifdef ALPHA_TEST
		if (albedoTex.a < 0.37)
			discard;
	#endif

	float3 albedo = albedoTex.rgb * MaterialColor;
#else
	float3 albedo = MaterialColor;
#endif

	float camDistance = length(MainCameraPosition - input.worldPosition.xyz);

	float shadow = getPssmShadow(input.worldPosition, camDistance, dotLighting, ShadowSampler, Sun);
	float3 lighting = (ambientColor + shadow * diffuse);
	float lightPower = (lighting.r + lighting.g + lighting.b) / 3;
	float4 outColor = float4(lighting * albedo, lightPower);

	PSOutput output;
    output.color = outColor;
	output.normals = float4(normal, 1);
	output.camDistance = float4(camDistance, 0, 0, 0);
	output.motionVectors = float4((input.previousPosition.xy / input.previousPosition.w - input.currentPosition.xy / input.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;
	return output;
}
