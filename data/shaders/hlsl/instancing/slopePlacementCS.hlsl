#include "hlsl/common/Random.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/grid/heightmapGridSampling.hlsl"

uint TexIdTerrainHeight;
float TerrainHeight;
float2 TerrainWorldMin;
float2 SectorWorldMin;
float SectorWorldSize;
uint MaxInstances;
float2 HeightmapUvMin;
float2 HeightmapUvSize;
uint Seed;
uint IndexCount;

cbuffer PlacementParams : register(b1)
{
	uint SamplesPerAxis;
	float Density;
	float MinNormalY;
	float MaxNormalY;
	float MinScale;
	float MaxScale;
	float HeightOffset;
	uint PlacementPadding;
};

RWStructuredBuffer<float4x4> InstanceBuffer : register(u0);
RWByteAddressBuffer IndirectCommand : register(u1);
SamplerState LinearWrapSampler : register(s0);

float SampleHeight(Texture2D<float> heightmap, float2 uv)
{
	return heightmap.SampleLevel(LinearWrapSampler, RemapGridTextureUV(uv), 0);
}

[numthreads(8, 8, 1)]
void main(uint3 groupThreadId : SV_GroupThreadID)
{
	if (groupThreadId.x == 0 && groupThreadId.y == 0)
	{
		IndirectCommand.Store(0, IndexCount);
		IndirectCommand.Store(4, 0);
		IndirectCommand.Store(8, 0);
		IndirectCommand.Store(12, 0);
		IndirectCommand.Store(16, 0);
	}
	GroupMemoryBarrierWithGroupSync();

	Texture2D<float> heightmap = GetTexture2D1f(TexIdTerrainHeight);
	uint sampleCount = max(SamplesPerAxis, 1);

	for (uint sampleY = groupThreadId.y; sampleY < sampleCount; sampleY += 8)
	{
		for (uint sampleX = groupThreadId.x; sampleX < sampleCount; sampleX += 8)
		{
			uint2 sample = uint2(sampleX, sampleY);
			float2 randomInput = float2(sample) + float2(Seed, Seed * 17u);
			float randomValue = RandomFrom2D(randomInput);
			if (randomValue > Density)
				continue;

			float2 jitter = float2(RandomFrom2D(randomInput.yx + 11.0f), RandomFrom2D(randomInput + 29.0f));
			float2 localUv = (float2(sample) + jitter) / sampleCount;
			float2 heightmapUv = HeightmapUvMin + localUv * HeightmapUvSize;
			float height = SampleHeight(heightmap, heightmapUv);

			float texel = 1.0f / 960.0f;
			float heightRight = SampleHeight(heightmap, heightmapUv + float2(texel, 0));
			float heightUp = SampleHeight(heightmap, heightmapUv + float2(0, texel));
			float worldTexel = SectorWorldSize / HeightmapUvSize.x / 960.0f;
			float3 tangent = float3(worldTexel, (heightRight - height) * TerrainHeight, 0);
			float3 bitangent = float3(0, (heightUp - height) * TerrainHeight, worldTexel);
			float normalY = normalize(cross(bitangent, tangent)).y;
			if (normalY < MinNormalY || normalY > MaxNormalY)
				continue;

			uint instanceIndex;
			IndirectCommand.InterlockedAdd(4, 1, instanceIndex);
			if (instanceIndex >= MaxInstances)
			{
				IndirectCommand.InterlockedMin(4, MaxInstances);
				continue;
			}

			float scale = lerp(MinScale, MaxScale, RandomFrom2D(randomInput + 47.0f));
			float angle = RandomFrom2D(randomInput + 71.0f) * 6.28318530718f;
			float sine;
			float cosine;
			sincos(angle, sine, cosine);
			float2 worldXZ = SectorWorldMin + localUv * SectorWorldSize;
			float worldY = TerrainWorldMin.y + height * TerrainHeight + HeightOffset;

			InstanceBuffer[instanceIndex] = float4x4(
				scale * cosine, 0, -scale * sine, 0,
				0, scale, 0, 0,
				scale * sine, 0, scale * cosine, 0,
				worldXZ.x, worldY, worldXZ.y, 1);
		}
	}
}
