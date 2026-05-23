#include "PostProcessCommon.hlsl"
#include "WorldReconstruction.hlsl"
#include "hlsl/vct/anisoSeparate/VoxelRayTracingCommon.hlsl"
#include "hlsl/common/Random.hlsl"

#ifndef VRT_NUM_RAYS
#define VRT_NUM_RAYS 16
#endif

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
float Time;
float ResId;

cbuffer SceneVoxelInfo : register(b1)
{
	AnisoSeparateSceneVoxelCbufferIndexed VoxelInfo;
};

Texture2D normalMap : register(t0);
Texture2D<float> depthMap : register(t1);

SamplerState PointSampler : register(s0);

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
	float t = saturate(dir.y * 0.75 + 0.25);
	float3 bottomColor = float3(0.1, 0.15, 0.2);
	float3 topColor = float3(0.3, 0.5, 0.9);
	return lerp(bottomColor, topColor, 1 - t) * 0.1f;
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

	float2 pixelSeed = input.TexCoord;
	[unroll]
	for (int r = 0; r < VRT_NUM_RAYS; r++)
	{
		float2 xi = Random2DFrom2D(pixelSeed + Time % 10 + float2(r * 7.239, r * 13.137));

		float3 dir = CosineWeightedHemisphere(xi, worldNormal, worldTangent, worldBinormal);

		float4 hit = RayTraceCascades(worldPosition, dir, 0, VoxelInfo);

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

	return float4(radiance, 1 - occlusion);
}
