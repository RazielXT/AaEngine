#include "PostProcessCommon.hlsl"
#include "WorldReconstruction.hlsl"
#include "hlsl/vct/anisoSeparate/VoxelRayTracingCommon.hlsl"
#include "hlsl/common/Random.hlsl"
#include "hlsl/sky/SkyColor.hlsl"

#ifndef VRT_NUM_RAYS
#define VRT_NUM_RAYS 4
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

cbuffer SkyParamsBuffer : register(b2)
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
	xi = saturate(xi); // guard sqrt(1 - xi.y) against negative inputs
	float phi = 2.0 * 3.14159265 * xi.x;
	float cosTheta = sqrt(1.0 - xi.y);
	float sinTheta = sqrt(xi.y);

	float3 localDir = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	float3 dir = T * localDir.x + B * localDir.y + N * localDir.z;
	float len = length(dir);
	return (len > 1e-6f) ? dir / len : normalize(N);
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
	// Align ray-start sampling with the half-res linear depth (linearDepthDownsample2 / DS2x),
	// which stores the TOP-LEFT full-res texel (2*p) of each 2x2 block. The interpolated TexCoord
	// lands on the half-res pixel center, i.e. full-res texel (2*p+1). Shift by half a full-res
	// texel toward the top-left so depth/normal/world line up with the depth used during upsampling.
	float2 sampleUv = input.TexCoord - 0.25f / float2(ViewportSize);

	float3 worldNormal = normalMap.Sample(PointSampler, sampleUv).rgb;
	float3 up = abs(worldNormal.y) < 0.999 ? float3(0,1,0) : float3(1,0,0);
	float3 worldTangent = normalize(cross(up, worldNormal));
	float3 worldBinormal = cross(worldNormal, worldTangent);

	if (all(worldNormal == float3(0,0,0))) return float4(0,0,0,1);

	float depth = depthMap.Sample(PointSampler, sampleUv).r;
	float3 worldPosition = ReconstructWorldPosition(sampleUv, depth, InvViewProjectionMatrix);

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

		// Instead of entirely random values, offset each ray into its own quadrant
		// Assuming r goes from 0 to 3
		float2 quadrantOffset = float2(float(r % 2) * 0.5f, float(r / 2) * 0.5f);

		// Remap your blue noise weight (0.0 to 1.0) into a tight 0.0 to 0.5 range, 
		// then slide it into its assigned quadrant
		float2 stratifiedXi = quadrantOffset + (noiseWeight * 0.5f);

		// Feed this perfectly distributed coordinate into your hemisphere function
		float3 dir = CosineWeightedHemisphere(stratifiedXi, worldNormal, worldTangent, worldBinormal);

		//float3 dir = CosineWeightedHemisphere(noiseWeight, worldNormal, worldTangent, worldBinormal);

		float4 hit = RayTraceCascades(worldPosition + worldNormal, dir, 0, VoxelInfo);

		if (hit.a > 0)
		{
			radiance += hit.rgb;
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

	return float4(radiance * 2, 1 - occlusion);
}
