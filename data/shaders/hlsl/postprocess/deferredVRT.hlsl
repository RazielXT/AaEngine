#include "PostProcessCommon.hlsl"
#include "WorldReconstruction.hlsl"
#include "hlsl/vct/anisoSeparate/VoxelRayTracingCommon.hlsl"
#include "hlsl/common/Random.hlsl"
#include "hlsl/sky/SkyColor.hlsl"

#ifndef VRT_NUM_RAYS
#define VRT_NUM_RAYS 2
#endif

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
float Time;
uint2 ViewportSize;
float ResId;

cbuffer SceneVoxelInfo : register(b1)
{
	AnisoSeparateSceneVoxelCbufferIndexed VoxelInfo;
};

cbuffer PSSMShadows : register(b2)
{
	SkyParams Sky;
}

Texture2D normalMap : register(t0);
Texture2D<float> depthMap : register(t1);
Texture2D<float> blueNoiseMap : register(t2);

SamplerState PointSampler : register(s0);
SamplerState LinearWrapSampler : register(s1);
SamplerState PointWrapSampler : register(s2);

float3 CosineWeightedHemisphere(float2 xi, float3 N, float3 T, float3 B)
{
	float phi = 2.0 * 3.14159265 * xi.x;
	float cosTheta = sqrt(1.0 - xi.y);
	float sinTheta = sqrt(xi.y);

	float3 localDir = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	return normalize(T * localDir.x + B * localDir.y + N * localDir.z);
}

float3 SkyColor(float3 dir)
{
	float3 sky = getSkyColor(dir, Sky, PointSampler);

	float t = saturate(dir.y * 0.75 + 0.25);
	float3 bottomColor = float3(0.1, 0.15, 0.2);
	return lerp(bottomColor, sky, 1 - t) * 0.3f;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float3 worldNormal = normalMap.Sample(PointSampler, input.TexCoord).rgb;
	float3 up = abs(worldNormal.y) < 0.999 ? float3(0,1,0) : float3(1,0,0);
	float3 worldTangent = normalize(cross(up, worldNormal));
	float3 worldBinormal = cross(worldNormal, worldTangent);

	if (all(worldNormal == float3(0,0,0))) return float4(0,0,0,1);

	float depth = depthMap.Sample(PointSampler, input.TexCoord).r;
	float3 worldPosition = ReconstructWorldPosition(input.TexCoord, depth, InvViewProjectionMatrix);

	float3 radiance = 0;
	float occlusion = 0;
	float2 noiseWeight;

	[unroll]
	for (int r = 0; r < VRT_NUM_RAYS; r++)
	{
	#ifdef VRT_WHITE_NOISE
		const float2 pixelSeed = input.TexCoord;
		float2 xi = Random2DFrom2D(pixelSeed + Time % 10 + float2(r * 0.239, r * 0.137));
		float2 xi2 = Random2DFrom2D(pixelSeed - Time % 3 + float2(r * 0.119, r * 0.437));
		noiseWeight = float2(xi.x, xi2.y);
	#else
		float2 blueNoiseUv = input.TexCoord * (ViewportSize / 128.f);
		float2 uvScramble = saturate(Random2DFrom2D(Time % 2 + float2(r * 0.239, r * 0.137)));
		float xi = blueNoiseMap.Sample(PointWrapSampler, blueNoiseUv + uvScramble);
		float xi2 = blueNoiseMap.Sample(PointWrapSampler, blueNoiseUv - uvScramble);
		noiseWeight = float2(xi, xi2);
	#endif

		float3 dir = CosineWeightedHemisphere(noiseWeight, worldNormal, worldTangent, worldBinormal);

		float4 hit = RayTraceCascades(worldPosition, dir, 0, VoxelInfo);

		if (hit.a > 0)
		{
			radiance += hit.rgb * 3;
			occlusion += 1.0;
		}
		else
		{
			radiance += SkyColor(dir);
		}
	}

	radiance /= VRT_NUM_RAYS;
	occlusion /= VRT_NUM_RAYS;

#ifdef VRT_OUTPUT_NOISE
	radiance = float3(noiseWeight, 0);
#endif

	return float4(radiance, 1 - occlusion);
}
