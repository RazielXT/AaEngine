#include "PostProcessCommon.hlsl"
#include "hlsl/sky/SkyParams.hlsl"
#include "hlsl/common/SceneRenderingState.hlsl"

float2 ViewportSizeInverse;
float Time;

Texture2D colorMap : register(t0);
Texture2D<float3> underwaterMap : register(t1);

StructuredBuffer<SceneRenderingStateParams> SceneRenderingState : register(t2);

cbuffer SkyParamsBuffer : register(b0)
{
	SkyParams Sky;
}

SamplerState LinearSampler : register(s0);
SamplerState LinearWrapSampler : register(s1);


float borderBlend(float2 uv, float edgeThreshold)
{
	// Calculate the distance from the UV coordinates to the nearest edge
	float left = uv.x;
	float right = 1.0 - uv.x;
	float top = uv.y;
	float bottom = 1.0 - uv.y;

	// Determine the minimum distance to an edge
	float minDist = min(min(left, right), min(top, bottom));

	// Perform linear interpolation (lerp) based on the dist / edgeThresholdance to the edge
	float alpha = saturate(minDist / edgeThreshold);

	return alpha;
}

float4 PSPreToneMappingFx(VS_OUTPUT input) : SV_TARGET
{
	float underwaterState = SceneRenderingState[0].Underwater;
	float isUnderwater = step(1.0f, underwaterState);

	float4 color = colorMap.Sample(LinearSampler, input.TexCoord);
	//color.rgb *= lerp(float3(1,1,1), SceneRenderingState[0].WaterColor * (0.2 + Sky.SunColor * 0.8), isUnderwater);

	return color;
}

float4 PSPostToneMappingFx(VS_OUTPUT input) : SV_TARGET
{
	float underwaterState = SceneRenderingState[0].Underwater;
	float isUnderwater = step(1.0f, underwaterState);
	float isDry = (1-underwaterState * underwaterState);

	float2 underwaterMapUv = input.TexCoord;
	underwaterMapUv.y -= Time*0.5 + isDry;
	float3 underwaterUvOffset = underwaterMap.Sample(LinearWrapSampler, underwaterMapUv);
	{
		float heightEnd = smoothstep(0,1,isDry);
		float w = saturate(underwaterUvOffset.z-heightEnd);
		
		if(w >0 && w < 0.2)
			w = lerp(w,lerp(15,0.2,w*5),heightEnd);
		
		underwaterUvOffset.xy = (underwaterUvOffset.xy - 0.5)*w*0.15f; 
	}
	//float underwaterUvOffsetStrength = 0.05f;
	//underwaterUvOffset = (underwaterUvOffset - 0.5f) * underwaterUvOffsetStrength;
	underwaterUvOffset *= borderBlend(input.TexCoord, 0.05f);
	//underwaterUvOffset *= underwaterState;

	float4 color = colorMap.Sample(LinearSampler, input.TexCoord + underwaterUvOffset.xy);

	return color;
}
