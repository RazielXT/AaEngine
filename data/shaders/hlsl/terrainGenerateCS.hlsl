uint4 BorderBlendIdx;
uint2 GridIdx;
uint GridSize;
uint TexIdTerrainDepth;
float2 Offset;
float GridScale;
float WorldSize;
float HeightSize;

// Define the TerrainVertex structure
struct TerrainVertex
{
    float3 pos;
    float2 uv;
    float3 normal;
    float3 tangent;
};

RWStructuredBuffer<TerrainVertex> vertexBuffer : register(u0);

Texture2D<float> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
SamplerState LinearWrapSampler : register(s0);

float getTerrainHeight(float2 coords)
{
    float height = GetTexture(TexIdTerrainDepth).SampleLevel(LinearWrapSampler, coords, 3).r;
    return height;
}

// Define shared memory for height values
groupshared float sharedHeights[18][18];

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID, uint3 groupThreadId : SV_GroupThreadID)
{
    uint index = id.x + id.y * GridSize;
	float invGridSize = GridScale / GridSize;

    // Calculate UV coordinates based on thread ID
    float2 localUv = float2(id.x / (float)(GridSize - 1), id.y / (float)(GridSize - 1));
	float2 uv = localUv;
    uv *= GridScale;
	uv += Offset;

    // Calculate shared memory indices
    uint sharedX = groupThreadId.x + 1;
    uint sharedY = groupThreadId.y + 1;

    // Sample height and store in shared memory
	float height = getTerrainHeight(uv) * HeightSize;

    sharedHeights[sharedY][sharedX] = height;

	// Skip last group vertex, vertices are odd sized to have tiling grid pattern |<|>|<|>|
	if (id.x == GridSize || id.y == GridSize)
		return;

    // Load neighboring heights for border threads
    if (sharedX == 16) {
        sharedHeights[sharedY][17] = getTerrainHeight(float2(uv.x + invGridSize, uv.y)) * HeightSize;
    }
    if (sharedY == 16) {
        sharedHeights[17][sharedX] = getTerrainHeight(float2(uv.x, uv.y + invGridSize)) * HeightSize;
    }
    if (sharedX == 1) {
        sharedHeights[sharedY][0] = getTerrainHeight(float2(uv.x - invGridSize, uv.y)) * HeightSize;
    }
    if (sharedY == 1) {
        sharedHeights[0][sharedX] = getTerrainHeight(float2(uv.x, uv.y - invGridSize)) * HeightSize;
    }

    // Synchronize threads to ensure all heights are loaded
    GroupMemoryBarrierWithGroupSync();

	// Stich border seams
	if ((id.x == BorderBlendIdx.x || id.x == BorderBlendIdx.y) && groupThreadId.y % 2 == 1)
	{
		height = (sharedHeights[sharedY - 1][sharedX] + sharedHeights[sharedY + 1][sharedX]) / 2.f;
		sharedHeights[sharedY][sharedX] = height;
	}
	else if ((id.y == BorderBlendIdx.z || id.y == BorderBlendIdx.w) && groupThreadId.x % 2 == 1)
	{
		height = (sharedHeights[sharedY][sharedX - 1] + sharedHeights[sharedY][sharedX + 1]) / 2.f;
		sharedHeights[sharedY][sharedX] = height;
	}

	GroupMemoryBarrierWithGroupSync();

    // Calculate tangents and binormals
	float quadWidth = WorldSize * invGridSize;
    float3 tangent = float3(0.0f, sharedHeights[sharedY + 1][sharedX] - height, quadWidth);
    float3 binormal = float3(quadWidth, sharedHeights[sharedY][sharedX + 1] - height, 0.0f);
    
    // Calculate normal using cross product
    float3 normal = normalize(cross(tangent, binormal));
	tangent = normalize(tangent);

    // Calculate vertex position
    float3 pos = float3(localUv.x * GridScale * WorldSize, height, localUv.y * GridScale * WorldSize);
    
    // Create the TerrainVertex
    TerrainVertex vertex = { pos, uv * 30 * 30, normal, tangent };
    
    // Write vertex to buffer
    vertexBuffer[index] = vertex;
}
