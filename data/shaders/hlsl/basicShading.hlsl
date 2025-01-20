#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 PreviousWorldMatrix;
float4x4 WorldMatrix;
float3 MaterialColor;
uint TexIdDiffuse;
float3 CameraPosition;
uint2 ViewportSize;

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
    float2 uv : TEXCOORD;
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
	float2 uv : TEXCOORD1;
	float4 worldPosition : TEXCOORD2;
	float4 previousPosition : TEXCOORD3;
	float4 currentPosition : TEXCOORD4;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

#ifdef INSTANCED
	result.worldPosition = mul(input.position, InstancingBuffer[input.instanceID]);
    result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = mul(input.normal, (float3x3)InstancingBuffer[input.instanceID]);
	result.previousPosition = result.position; // no support
	result.currentPosition = result.position;
#else
	result.worldPosition = mul(input.position, WorldMatrix);
    result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = mul(input.normal, (float3x3)WorldMatrix);
	float4 previousWorldPosition = mul(input.position, PreviousWorldMatrix);
	result.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	result.currentPosition = result.position;
#endif

#ifdef USE_VC
    result.color = input.color;
#endif

	result.uv = input.uv;

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
	float4 albedo = GetTexture(TexIdDiffuse).Sample(sampler, input.uv);
	albedo.rgb *= MaterialColor;

	float camDistance = length(CameraPosition - input.worldPosition.xyz);

	float shadow = getPssmShadow(input.worldPosition, camDistance, dotLighting, ShadowSampler, Sun);
	float3 lighting = (ambientColor + shadow * diffuse);
	float lightPower = (lighting.r + lighting.g + lighting.b) / 3;
	float4 outColor = float4(lighting * albedo.rgb, lightPower);

	PSOutput output;
    output.color = outColor;
	output.normals = float4(normal, 1);
	output.camDistance = float4(camDistance / 10000, 0, 0, 0);
	output.motionVectors = float4((input.previousPosition.xy / input.previousPosition.w - input.currentPosition.xy / input.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;
	return output;
}
