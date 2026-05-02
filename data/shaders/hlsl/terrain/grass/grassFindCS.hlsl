#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/Random.hlsl"
#include "grassCommon.hlsl"

uint TexIdTerrainDepth;
float TerrainHeight;
float2 TerrainOffset;
float2 ChunkWorldSize;
float2 SubUvOffset;
float2 SubUvScale;

RWStructuredBuffer<GrassInfo> infoBuffer : register(u0);
RWByteAddressBuffer counterBuffer : register(u1);
SamplerState LinearWrapSampler : register(s0);

float2 RemapGridTextureUV(float2 gridUV)
{
	float margin = 32.0f / 1024.0f;
	float content = 960.0f / 1024.0f;
	return margin + (gridUV * content);
}

uint getGrassInfo(out GrassInfo info, float2 coords)
{
	float2 heightmapUv = SubUvOffset + coords * SubUvScale;
	float2 texCoords = RemapGridTextureUV(heightmapUv);

	Texture2D<float> heightmap = GetTexture2D1f(TexIdTerrainDepth);
	float height = heightmap.SampleLevel(LinearWrapSampler, texCoords, 0);
	float heightRight = heightmap.SampleLevel(LinearWrapSampler, texCoords + float2(1 / 1024.f, 0), 0);
	float heightUp = heightmap.SampleLevel(LinearWrapSampler, texCoords + float2(0, 1 / 1024.f), 0);

	float3 tangent = float3(1.0, (heightRight - height) * 1024, 0.0);
	float3 bitangent = float3(0.0, (heightUp - height) * 1024, 1.0);
	float3 normal = normalize(cross(bitangent, tangent));

	info.position.xz = (coords - 0.5f) * ChunkWorldSize;
	info.position.y = (height - 0.5f) * TerrainHeight;
	info.position.xz += TerrainOffset;

	float rnd = RandomFrom2D(coords.xy);
	info.rotation = rnd * 6.283185f;
	info.random = rnd;
	info.scale = (normal.y - 0.7) * 5 + rnd * 0.3;

	return normal.y > 0.75f ? 1 : 0;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	GrassInfo info;
	const uint ItemsPerThread = 4;
	const float ItemsTotalWidth = 4 * 8 * ItemsPerThread;

	for (uint x = 0; x < ItemsPerThread; x++)
		for (uint y = 0; y < ItemsPerThread; y++)
		{
			float2 coords = (dispatchThreadID.xy * ItemsPerThread + float2(x, y)) / ItemsTotalWidth;
			coords += (-0.5 + RandomFrom2D(float2(coords.x + y, coords.y + x))) * 2 / ItemsTotalWidth;

			if (getGrassInfo(info, coords) != 0)
			{
				uint index;
				counterBuffer.InterlockedAdd(0, 1, index);
				infoBuffer[index] = info;
			}
		}
}
