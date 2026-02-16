#include "ShadowsCommon.hlsl"

float4x4 ViewMatrix;
float4x4 ProjectionMatrix;
float3 CameraPosition;

Texture2D colorMap : register(t0);
SamplerState LinearBorderSampler : register(s0);

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 PSTraceGodray(VS_OUTPUT input) : SV_TARGET
{
	const int samples = 16;
	const float decay = 1.07;
	float2 uv = input.TexCoord;
	
	float3 camDirection = transpose(ViewMatrix)[2].xyz;
	float weight = dot(-camDirection, Sun.Direction);

	float4 lightPosition = float4(CameraPosition + Sun.Direction * -1000.f, 1);
	lightPosition = mul(lightPosition, ViewMatrix);
	lightPosition = mul(lightPosition, ProjectionMatrix);
	lightPosition.xyz /= lightPosition.w;

	float2 sunScreenPos;
	sunScreenPos.x = (lightPosition.x * 0.5 + 0.5);
	sunScreenPos.y = (1.0 - (lightPosition.y * 0.5 + 0.5));
	//sunScreenPos = float2(0.5f + (0.5f * lightPosition.x), 0.5f + (0.5f * -lightPosition.y));

	float2 deltaTexCoord = (uv - sunScreenPos);
	deltaTexCoord *= 1.0f / samples * 0.3;

	float3 col = colorMap.SampleLevel(LinearBorderSampler, uv, 0).rgb;
	float illuminationDecay = 1.0f;

	for (int i = 0; i < samples; i++)
	{
		uv -= deltaTexCoord;
		float3 sample = colorMap.SampleLevel(LinearBorderSampler, uv, 0).rgb;
		sample *= illuminationDecay * weight;
		col += sample;
		illuminationDecay *= decay;
	}

	return float4(col * 0.02, 1);
}
