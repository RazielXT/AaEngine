uint TexIdTerrainDepth;
float3 TerrainScale;
float2 TerrainOffset;

struct VegetationInfo
{
    float3 position;
    float rotation;
	float scale;
};

RWStructuredBuffer<VegetationInfo> infoBuffer : register(u0);
RWByteAddressBuffer counterBuffer : register(u1);

Texture2D<float> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
SamplerState LinearWrapSampler : register(s0);

float getRandom(float x, float z)
{
    float2 K1 = float2(
        23.14069263277926, // e^pi (Gelfond's constant)
         2.665144142690225 // 2^sqrt(2) (Gelfond–Schneider constant)
    );
    return frac(cos( dot(float2(x,z),K1) ) * 12345.6789 );
}

uint getVegetationInfo(out VegetationInfo info, float2 coords)
{
	Texture2D<float> heightmap = GetTexture(TexIdTerrainDepth);
    float height = heightmap.SampleLevel(LinearWrapSampler, coords, 1);
    float heightRight = heightmap.SampleLevel(LinearWrapSampler, coords + float2(1 / TerrainScale.x, 0), 1);
    float heightUp = heightmap.SampleLevel(LinearWrapSampler, coords + float2(0, 1 / TerrainScale.z), 1);

    // Calculate tangent and bitangent vectors
    float3 tangent = float3(1.0, (heightRight - height) * TerrainScale.y, 0.0);
    float3 bitangent = float3(0.0, (heightUp - height) * TerrainScale.y, 1.0);

    // Estimate the normal vector using cross product and normalize it
    float3 normal = normalize(cross(bitangent, tangent));
	
	info.position.xz = coords - 0.5f;
	info.position.y = height - 0.5f;
	info.position *= TerrainScale;

	info.rotation = 0;
	info.scale = 15 + getRandom(coords.x, coords.y) * 25;

    return normal.y > 0.85f ? 1 : 0;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    VegetationInfo info;
	const uint ItemsPerThread = 8;
	const float ItemsTotalWidth = 4 * 8 * ItemsPerThread;

	for (uint x = 0; x < ItemsPerThread; x++)
		for (uint y = 0; y < ItemsPerThread; y++)
		{
			float2 coords = (dispatchThreadID.xy * ItemsPerThread + float2(x, y)) / ItemsTotalWidth;
			coords += getRandom(coords.x + y, coords.y + x) * 2 / ItemsTotalWidth;

			if (getVegetationInfo(info, coords) != 0)
			{
				uint index;
				counterBuffer.InterlockedAdd(0, 1, index);
				infoBuffer[index] = info;
			}
		}
}
