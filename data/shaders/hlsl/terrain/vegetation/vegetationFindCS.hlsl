#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/Random.hlsl"
#include "vegetationCommon.hlsl"

uint TexIdTerrainDepth;
float TerrainHeight;
float2 TerrainOffset;
float2 ChunkWorldSize;
float2 SubUvOffset;
float2 SubUvScale;

RWStructuredBuffer<VegetationInfo> infoBuffer : register(u0);
RWStructuredBuffer<SubgroupMeta> subgroupMetaBuffer : register(u1);
SamplerState LinearWrapSampler : register(s0);

float2 RemapGridTextureUV(float2 gridUV)
{
	// We want to skip 32 pixels on the left, and use the next 960 pixels
	float margin = 32.0f / 1024.0f; // 0.03125
	float content = 960.0f / 1024.0f; // 0.9375

	return margin + (gridUV * content);
}

uint getVegetationInfo(out VegetationInfo info, float2 coords)
{
	// Map local coords [0,1] to the sub-region of the terrain heightmap
	float2 heightmapUv = SubUvOffset + coords * SubUvScale;
	float2 texCoords = RemapGridTextureUV(heightmapUv);

	Texture2D<float> heightmap = GetTexture2D1f(TexIdTerrainDepth);
	float height = heightmap.SampleLevel(LinearWrapSampler, texCoords, 0);
	float heightRight = heightmap.SampleLevel(LinearWrapSampler, texCoords + float2(1 / 1024.f, 0), 0);
	float heightUp = heightmap.SampleLevel(LinearWrapSampler, texCoords + float2(0, 1 / 1024.f), 0);

	// Calculate tangent and bitangent vectors
	float3 tangent = float3(1.0, (heightRight - height) * 1024, 0.0);
	float3 bitangent = float3(0.0, (heightUp - height) * 1024, 1.0);

	// Estimate the normal vector using cross product and normalize it
	float3 normal = normalize(cross(bitangent, tangent));

	// Position: coords [0,1] map to one vegetation chunk
	info.position.xz = (coords - 0.5f) * ChunkWorldSize;
	info.position.y = (height - 0.5f) * TerrainHeight;
	info.position.xz += TerrainOffset;

	info.rotation = 0;
	info.random = RandomFrom2D(coords.xy);
	info.scale = 25 + info.random * 35;

	return normal.y > 0.82f ? 1 : 0;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	VegetationInfo info;
	const uint ItemsPerThread = 8;
	const float ItemsTotalWidth = 4 * 8 * ItemsPerThread;

	// Each subgroup covers 32 positions per dim, each thread covers 8 items, so 4 threads per subgroup dim
	uint2 subgroupIdx = dispatchThreadID.xy / 4;
	uint subgroupLinear = subgroupIdx.y * SubgroupsPerDim + subgroupIdx.x;

	for (uint x = 0; x < ItemsPerThread; x++)
		for (uint y = 0; y < ItemsPerThread; y++)
		{
			float2 coords = (dispatchThreadID.xy * ItemsPerThread + float2(x, y)) / ItemsTotalWidth;
			coords += (-0.5 + RandomFrom2D(float2(coords.x + y, coords.y + x))) * 2 / ItemsTotalWidth;

			if (getVegetationInfo(info, coords) != 0)
			{
				int index;
				InterlockedAdd(subgroupMetaBuffer[subgroupLinear].counter, 1, index);

				if ((uint)index < MaxItemsPerSubgroup)
				{
					infoBuffer[subgroupLinear * MaxItemsPerSubgroup + index] = info;

					int posYInt = int(info.position.y * 100.0);
					InterlockedMin(subgroupMetaBuffer[subgroupLinear].minY, posYInt);
					InterlockedMax(subgroupMetaBuffer[subgroupLinear].maxY, posYInt);
				}
			}
		}
}
