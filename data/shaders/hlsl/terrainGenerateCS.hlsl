uint4 BorderBlendIdx;
int2 WorldGridOffset;
float InvWorldGridSize;
uint GridScale; // lod0 = 1, lod2 = 2, lod3 = 4 etc
uint GridSize;	//65
float HeightSize;
float QuadUnitSize;
float TerrainScale;
uint TexIdTerrainDepth;

// Define the TerrainVertex structure
struct TerrainVertex
{
    float3 pos;
    float3 normal;
    float3 tangent;	
    float4 normalHeightLod;
    float3 tangentLod;
};

RWStructuredBuffer<TerrainVertex> vertexBuffer : register(u0);

Texture2D<float> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
SamplerState LinearWrapSampler : register(s0);

float getTerrainHeight(float2 coords)
{
    float height = GetTexture(TexIdTerrainDepth).SampleLevel(LinearWrapSampler, (coords * TerrainScale) - 0.5, 1).r;
    return height * HeightSize;
}

// Define shared memory for height values 16x16 (17x17 for last group) + edges
groupshared float sharedHeights[19][19];
groupshared float3 sharedNormals[19][19];
groupshared float3 sharedTangents[19][19];

[numthreads(17, 17, 1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
	//we create 65 vertices (64 quads) 16/16/16/17, so SV_DispatchThreadID from 17 threads wont align
	uint3 id = groupId * 16 + groupThreadId;

    // Calculate UV coordinates based on thread ID
	int2 worldIdx = uint2(WorldGridOffset.x + id.x * GridScale, WorldGridOffset.y + id.y * GridScale);
	float2 uv = worldIdx * InvWorldGridSize;

    // Calculate shared memory indices
    uint sharedX = groupThreadId.x + 1;
    uint sharedY = groupThreadId.y + 1;

    // Sample height and store in shared memory
	float height = getTerrainHeight(uv);

    sharedHeights[sharedY][sharedX] = height;

	float invGridSize = GridScale * InvWorldGridSize;
    // Load neighboring heights for border threads
	if (sharedX == 1) {
        sharedHeights[sharedY][0] = getTerrainHeight(float2(uv.x - invGridSize, uv.y));
    }
    if (sharedY == 1) {
        sharedHeights[0][sharedX] = getTerrainHeight(float2(uv.x, uv.y - invGridSize));
    }
	// This is only needed in last group, otherwise last group thread filled it and returned above
    if (sharedX == 17) {
        sharedHeights[sharedY][18] = getTerrainHeight(float2(uv.x + invGridSize, uv.y));
    }
    if (sharedY == 17) {
        sharedHeights[18][sharedX] = getTerrainHeight(float2(uv.x, uv.y + invGridSize));
    }

	GroupMemoryBarrierWithGroupSync();

	float quadWidth = GridScale * QuadUnitSize;

    // Calculate tangents and binormals
    float3 tangent = float3(0.0f, sharedHeights[sharedY + 1][sharedX] - height, quadWidth);
    float3 binormal = float3(quadWidth, sharedHeights[sharedY][sharedX + 1] - height, 0.0f);
    
    // Calculate normal using cross product
    float3 normal = normalize(cross(tangent, binormal));
	tangent = normalize(tangent);

	float lodHeight = height;

	GroupMemoryBarrierWithGroupSync();

	if (groupThreadId.y % 2 == 1)
	{
		lodHeight = (sharedHeights[sharedY - 1][sharedX] + sharedHeights[sharedY + 1][sharedX]) / 2.f;
		sharedHeights[sharedY][sharedX] = lodHeight;
	}

	/*
	{
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
	}*/

	GroupMemoryBarrierWithGroupSync();
	
	if (groupThreadId.x % 2 == 1)
	{
		if (groupThreadId.y % 2 == 0)
		{
			lodHeight = (sharedHeights[sharedY][sharedX - 1] + sharedHeights[sharedY][sharedX + 1]) / 2.f;
		}
		else
		{
			if (groupThreadId.y % 4 == groupThreadId.x % 4)
			{
				lodHeight = (sharedHeights[sharedY - 1][sharedX - 1] + sharedHeights[sharedY + 1][sharedX + 1]) / 2.f;
			}
			else
			{
				lodHeight = (sharedHeights[sharedY + 1][sharedX - 1] + sharedHeights[sharedY - 1][sharedX + 1]) / 2.f;
			}
		}

		sharedHeights[sharedY][sharedX] = lodHeight;
	}

	GroupMemoryBarrierWithGroupSync();

    // Calculate tangents and binormals
    float3 lodTangent = float3(0.0f, sharedHeights[sharedY + 1][sharedX] - lodHeight, quadWidth);
    float3 lodBinormal = float3(quadWidth, sharedHeights[sharedY][sharedX + 1] - lodHeight, 0.0f);
    
    // Calculate normal using cross product
    float3 lodNormal = normalize(cross(lodTangent, lodBinormal));
	lodTangent = normalize(lodTangent);

	sharedNormals[sharedY][sharedX] = lodNormal;
	sharedTangents[sharedY][sharedX] = lodTangent;

	if (sharedX == 1)
	{
		sharedNormals[sharedY][0] = lodNormal;
		sharedTangents[sharedY][0] = lodTangent;
    }
    if (sharedY == 1)
	{
		sharedNormals[0][sharedX] = lodNormal;
		sharedTangents[0][sharedX] = lodTangent;
    }
	if (sharedX == 17)
	{
		sharedNormals[sharedY][18] = lodNormal;
		sharedTangents[sharedY][18] = lodTangent;
    }
    if (sharedY == 17)
	{
		sharedNormals[18][sharedX] = lodNormal;
		sharedTangents[18][sharedX] = lodTangent;
    }

	// To make 65 vertices, only last group edge adds 17 and other groups 16
	if ((sharedX == 17 && id.x != 64) || (sharedY == 17 && id.y != 64))
		return;

	GroupMemoryBarrierWithGroupSync();

	if (groupThreadId.y % 2 == 1)
	{
		lodNormal = (sharedNormals[sharedY - 1][sharedX] + sharedNormals[sharedY + 1][sharedX]) / 2.f;
		sharedNormals[sharedY][sharedX] = lodNormal;
		
		lodTangent = (sharedTangents[sharedY - 1][sharedX] + sharedTangents[sharedY + 1][sharedX]) / 2.f;
		sharedTangents[sharedY][sharedX] = lodTangent;
	}

	GroupMemoryBarrierWithGroupSync();

	if (groupThreadId.x % 2 == 1)
	{
		if (groupThreadId.y % 2 == 0)
		{
			lodNormal = (sharedNormals[sharedY][sharedX - 1] + sharedNormals[sharedY][sharedX + 1]) / 2.f;
			lodTangent = (sharedTangents[sharedY][sharedX - 1] + sharedTangents[sharedY][sharedX + 1]) / 2.f;
		}
		else
		{
			if (groupThreadId.y % 4 == groupThreadId.x % 4)
			{
				lodNormal = (sharedNormals[sharedY - 1][sharedX - 1] + sharedNormals[sharedY + 1][sharedX + 1]) / 2.f;
				lodTangent = (sharedTangents[sharedY - 1][sharedX - 1] + sharedTangents[sharedY + 1][sharedX + 1]) / 2.f;
			}
			else
			{
				lodNormal = (sharedNormals[sharedY + 1][sharedX - 1] + sharedNormals[sharedY - 1][sharedX + 1]) / 2.f;
				lodTangent = (sharedTangents[sharedY + 1][sharedX - 1] + sharedTangents[sharedY - 1][sharedX + 1]) / 2.f;
			}
		}
	}

	// Far LOD
	if (GridScale >= 16)
	{
		lodHeight = height;
		lodNormal = normal;
		lodTangent = tangent;
	}

    // Calculate vertex position
    float2 localUv = float2(id.x / (float)(GridSize - 1), id.y / (float)(GridSize - 1));
    float3 pos = float3(localUv.x * (GridSize - 1) * quadWidth, height, localUv.y * (GridSize - 1) * quadWidth);

    // Create the TerrainVertex
    TerrainVertex vertex = { pos, normal, tangent, float4(lodNormal, height - lodHeight), lodTangent };
    
    // Write vertex to buffer
    uint index = id.x + id.y * GridSize;
    vertexBuffer[index] = vertex;
}
