#include "hlsl/postprocess/PostProcessCommon.hlsl"
#include "hlsl/postprocess/WorldReconstruction.hlsl"
#include "hlsl/sky/SkyParams.hlsl"

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
float padding;
float2 ViewportSizeInverse;

Texture2D sceneTexture : register(t0);
Texture2D waterTexture : register(t1);
Texture2D reflectionsTexture : register(t2);
Texture2D waterNormal : register(t3);
Texture2D causticsTexture : register(t4);
Texture2D depthTexture : register(t5);

struct SceneRenderingStateParams
{
	float Underwater;
};
StructuredBuffer<SceneRenderingStateParams> SceneRenderingState : register(t6);

cbuffer SkyParamsBuffer : register(b1)
{
	SkyParams Sky;
}

SamplerState LinearSampler : register(s0);
SamplerState LinearBorderSampler : register(s1);
SamplerState DepthSampler : register(s2);

float2 MirrorUV(float2 v)
{
	float2 a = abs(v); // handles negative side
	return 1.0 - abs(a - 1.0); // mirrors around 1
}

float3 getCameraVector(float2 ScreenUV)
{
	float worldZ = depthTexture.Sample(DepthSampler, ScreenUV).r;
	float3 worldPosition = ReconstructWorldPosition(ScreenUV, worldZ, InvViewProjectionMatrix);
	return normalize(worldPosition - CameraPosition);
}

float3 getWaterLighting(float3 cameraVector, float3 normal, float occlusion)
{
	float3 ReflectionVector = reflect(cameraVector, normal);

	float isUnderwater = step(1.0f, SceneRenderingState[0].Underwater);
	if (isUnderwater == 1)
		cameraVector.y *= -1;

	float3 V = -cameraVector;
	float3 R = reflect(Sky.SunDirection, normal);
	float specular = pow( saturate( dot( R, V ) ), 128 );

	return specular * Sky.SunColor * 20 * (1 - occlusion);
}

float4 PSWaterApply(VS_OUTPUT input) : SV_TARGET
{
	float3 normal = waterNormal.Load(int3(input.Position.xy,0)).xyz;

	float3 cameraVector = getCameraVector(input.TexCoord);

	float fresnel = saturate((1 - abs(dot(cameraVector, normal))));
	fresnel = pow(fresnel,2);

	float2 refraction = normal.xx * 0.3f;
	float4 waterRefr = waterNormal.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0);
	refraction *= saturate(waterRefr.y * 10);

	float4 sceneColor = sceneTexture.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0);
	sceneColor.rgb += causticsTexture.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0).rrr;

	float reflOffset = normal.x * 0.15f;
	float2 reflectionUv = MirrorUV(input.TexCoord + float2(reflOffset, 0));

	float4 reflections = reflectionsTexture.SampleLevel(LinearSampler, reflectionUv, 0);
	float4 water = waterTexture.Load(int3(input.Position.xy,0));

	water.rgb = lerp(water.rgb, reflections.rgb, fresnel);
	water.rgb += getWaterLighting(cameraVector, normal, reflections.a);

	sceneColor.rgb = lerp(sceneColor.rgb, water.rgb, water.a);

	return sceneColor;
}
