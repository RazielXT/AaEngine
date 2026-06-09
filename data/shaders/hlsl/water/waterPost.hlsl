#include "hlsl/postprocess/PostProcessCommon.hlsl"
#include "hlsl/postprocess/WorldReconstruction.hlsl"
#include "hlsl/sky/SkyParams.hlsl"
#include "hlsl/common/BlueNoise.hlsl"
#include "hlsl/common/SceneRenderingState.hlsl"

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
float padding;
float2 ViewportSizeInverse;

Texture2D sceneTexture : register(t0);
Texture2D waterTexture : register(t1);
Texture2D reflectionsTexture : register(t2);
Texture2D waterNormal : register(t3);
Texture2D depthTexture : register(t4);
Texture2D waterDepthTexture : register(t5);
Texture2D godrayTexture : register(t6);

StructuredBuffer<SceneRenderingStateParams> SceneRenderingState : register(t7);

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

float3 getSceneWorldPosition(float2 ScreenUV)
{
	float worldZ = depthTexture.Sample(DepthSampler, ScreenUV).r;
	return ReconstructWorldPosition(ScreenUV, worldZ, InvViewProjectionMatrix);
}

float3 getWaterWorldPosition(float2 ScreenUV)
{
	float worldZ = waterDepthTexture.Sample(DepthSampler, ScreenUV).r;
	return ReconstructWorldPosition(ScreenUV, worldZ, InvViewProjectionMatrix);
}

float3 getWaterLighting(float3 cameraVector, float3 normal, float occlusion, float isUnderwater)
{
	float3 ReflectionVector = reflect(cameraVector, normal);


	float3 V = -cameraVector;
	float3 R = reflect(Sky.SunDirection, normal);
	float specular = pow( saturate( dot( R, V ) ), 128 );

	return specular * Sky.SunColor * 20 * (1 - occlusion) * (1 - isUnderwater);
}

float4 PSWaterApply(VS_OUTPUT input) : SV_TARGET
{
	float3 normal = waterNormal.Load(int3(input.Position.xy,0)).xyz;
	float isUnderwater = step(1.0f, SceneRenderingState[0].Underwater);
	float2 refraction = normal.xx * 0.05f * (1 + isUnderwater * 3);

	float4 waterRefr = waterNormal.SampleLevel(LinearBorderSampler, saturate(input.TexCoord + refraction), 0);
	refraction *= saturate(waterRefr.y * 2);

	float2 refractionUv = MirrorUV(input.TexCoord + refraction);

	float3 worldPosition = getSceneWorldPosition(refractionUv);
	float camDistance = length(worldPosition - CameraPosition);
	float3 cameraVector = (worldPosition - CameraPosition) / camDistance;

	float fresnel = saturate((1 - abs(dot(cameraVector, normal))));
	fresnel = pow(fresnel,2);

	float3 sceneColor = sceneTexture.SampleLevel(LinearSampler, refractionUv, 0).rgb;
	//sceneColor += (1 - isUnderwater) * causticsTexture.SampleLevel(LinearSampler, refractionUv, 0).rrr;

	float reflOffset = normal.x * 0.1f;
	float2 reflectionUv = MirrorUV(input.TexCoord + reflOffset);

	float reflOffsetStep = step(1.0f, waterTexture.SampleLevel(LinearSampler, reflectionUv, 0).a > 0);
	reflectionUv = MirrorUV(input.TexCoord + reflOffset * reflOffsetStep);

	float3 waterWorldPosition = getWaterWorldPosition(refractionUv);
	worldPosition = lerp(worldPosition, waterWorldPosition, isUnderwater);
	waterWorldPosition = lerp(waterWorldPosition, CameraPosition, isUnderwater);

	float waterDepth = length(waterWorldPosition - worldPosition);

	float3 fogAtmDir = normalize(CameraPosition - worldPosition);
	float3 fogColor = SceneRenderingState[0].WaterColor * Sky.SunColor * (1 - fogAtmDir.y * 0.8f);
	float fogDensity = 0.003f;
	float fogFactor = 1.0 - exp(-waterDepth * waterDepth * fogDensity);
	fogFactor = max(fogFactor, isUnderwater * 0.15f);

	float4 reflections = reflectionsTexture.SampleLevel(LinearSampler, reflectionUv, 0);
	float4 water = waterTexture.SampleLevel(LinearSampler, refractionUv, 0);
	water.rgb = lerp(water.rgb, reflections.rgb, saturate(fresnel - isUnderwater));
	water.rgb += getWaterLighting(cameraVector, normal, reflections.a, isUnderwater);

	float noise = BlueNoise(input.Position.xy);
	float ditherStrength = 1 * (noise - 0.5) / 255.0;
	fogColor.rgb += ditherStrength;

	float3 finalColor = lerp(sceneColor, fogColor, saturate(fogFactor));
	finalColor = lerp(finalColor, water.rgb, saturate(water.a - saturate(fogFactor) * isUnderwater));

	finalColor.rgb += godrayTexture.SampleLevel(LinearSampler, input.TexCoord, 0).xxx * Sky.SunColor * saturate(-Sky.SunDirection.y);
	float depthDarkness = saturate(1 - 0.95f * saturate(SceneRenderingState[0].UnderwaterDepth / 150.f)) + ditherStrength;
	finalColor.rgb *= depthDarkness;

	return float4(finalColor, 1);
}
