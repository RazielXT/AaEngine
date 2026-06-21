#pragma once

#include "WorldReconstruction.hlsl"
#include "hlsl/vct/anisoSeparate/VoxelRayTracingCommon.hlsl"
#include "hlsl/common/Random.hlsl"

#define DEFERRED_VRT_TRACE_THREADS 64

struct DeferredVrtRayData
{
	uint packedData;
	uint packedNormal;
	float depth;
	float tCurrent;
	float3 rayDirection;
};

struct DeferredVrtRayResult
{
	float4 radianceOcclusion;
};

uint PackPixel(uint2 pixel)
{
	return (pixel.x & 0xfffu) | ((pixel.y & 0xfffu) << 12);
}

uint2 UnpackPixel(uint packedData)
{
	return uint2(packedData & 0xfffu, (packedData >> 12) & 0xfffu);
}

uint PixelIndex(uint2 pixel, uint2 viewportSize)
{
	return pixel.y * viewportSize.x + pixel.x;
}

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

float3 DeferredVrtSkyColor(float3 dir)
{
	float t = saturate(dir.y * 0.75 + 0.25);
	float3 topColor = float3(0.12, 0.19, 0.32);
	float3 bottomColor = float3(0.1, 0.15, 0.2);
	return lerp(bottomColor, topColor, 1.0 - t) * 0.3f;
}

uint2 ResolveSourcePixel(uint2 pixel, uint2 viewportSize, uint2 sourceSize)
{
	float2 uv = (float2(pixel) + 0.5f) / float2(viewportSize);
	return min(uint2(uv * float2(sourceSize)), sourceSize - 1);
}

float LoadDeferredVrtDepth(uint2 pixel, uint2 viewportSize, uint TexIdDepth)
{
	Texture2D<float> depthMap = ResourceDescriptorHeap[TexIdDepth];
	uint width, height;
	depthMap.GetDimensions(width, height);
	return depthMap.Load(uint3(ResolveSourcePixel(pixel, viewportSize, uint2(width, height)), 0)).r;
}

float3 ReconstructDeferredVrtWorldPosition(uint2 pixel, uint2 viewportSize, float depth, float4x4 invViewProjectionMatrix)
{
	float2 uv = (float2(pixel) + 0.5f) / float2(viewportSize);
	return ReconstructWorldPosition(uv, depth, invViewProjectionMatrix);
}

float3 LoadDeferredVrtNormal(uint2 pixel, uint2 viewportSize, uint TexIdNormal)
{
	Texture2D<float4> normalMap = ResourceDescriptorHeap[TexIdNormal];
	uint width, height;
	normalMap.GetDimensions(width, height);
	return normalMap.Load(uint3(ResolveSourcePixel(pixel, viewportSize, uint2(width, height)), 0)).rgb;
}

float4 DeferredVrtMissResult(float3 rayDirection)
{
	return float4(rayDirection, 0.0f);
}

float4 DeferredVrtHitResult(float3 radiance)
{
	return float4(radiance * 6.0f, 1.0f);
}
