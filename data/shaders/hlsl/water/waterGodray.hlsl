#include "hlsl/sky/SkyParams.hlsl"
#include "hlsl/common/SceneRenderingState.hlsl"

float4x4 ViewMatrix;
float4x4 ProjectionMatrix;
float3 CameraPosition;

Texture2D colorMap : register(t0);

StructuredBuffer<SceneRenderingStateParams> SceneRenderingState : register(t1);

SamplerState LinearBorderSampler : register(s0);

cbuffer SkyParamsBuffer : register(b1)
{
	SkyParams Sky;
}

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 PSTraceGodray(VS_OUTPUT input) : SV_TARGET
{
	const int samples = 16;
	const float decay = 1.04;
	float2 uv = input.TexCoord;
	
	float3 camDirection = transpose(ViewMatrix)[2].xyz;
	float weight = SceneRenderingState[0].Underwater;

	float4 lightPosition = float4(CameraPosition + Sky.SunDirection * -100.f, 1);
	lightPosition = mul(lightPosition, ViewMatrix);
	lightPosition = mul(lightPosition, ProjectionMatrix);
	lightPosition.xyz /= lightPosition.w;

	float2 sunScreenPos;
	sunScreenPos.x = (lightPosition.x * 0.5 + 0.5);
	sunScreenPos.y = (1.0 - (lightPosition.y * 0.5 + 0.5));

	float2 deltaTexCoord = normalize(uv - sunScreenPos) * sign(dot(-camDirection, Sky.SunDirection)) * saturate(-Sky.SunDirection.y);
	deltaTexCoord *= 1.0f / samples * 0.13;

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

	return float4(col * 0.1, 1);
}
