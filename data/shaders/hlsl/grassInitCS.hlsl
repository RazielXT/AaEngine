float4x4 InvProjectionMatrix;
float3 BoundsMin;
float3 BoundsMax;
uint GrassCount;
uint GrassCountRows;
uint TexIdTerrainDepth;
uint TexIdTerrainColor;

struct GrassVertex { float3 pos; float3 color; };

RWStructuredBuffer<GrassVertex> grassPos : register(u0);
RWByteAddressBuffer counterBuffer : register(u1);

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
SamplerState g_sampler : register(s0);


float2 getGrassCoords(float3 position)
{
	float3 scope = (position - BoundsMin) / (BoundsMax - BoundsMin);

	float2 coords = scope.xz;
	coords.y = 1 - coords.y;
	
	return coords;
}

float getRandom(float seed)
{
	return ((abs(seed) * 373) % 100) / 100.0;
}

float getRandomFloat(float from, float to)
{
	return lerp(from, to, getRandom(from*to));
}

void createGrassPositions(uint index, out float3 pos1, out float3 pos2)
{
	uint x = index / GrassCountRows;
	uint z = index % GrassCountRows;
	
	float width = 2;
	float halfWidth = width / 2;

	float xPos = BoundsMin.x + getRandomFloat(x * width - halfWidth, x * width + halfWidth);
	float zPos = BoundsMin.z + getRandomFloat(z * width - halfWidth, z * width + halfWidth);
	
	float angle = getRandom(x * z * xPos * zPos) * 6.28;
	float xTrans = cos(angle) * width;
	float zTrans = sin(angle) * width;

	xPos += getRandom(xPos * zPos) * width;
	
	pos1 = float3(xPos - xTrans, 0, zPos - zTrans);
	pos2 = float3(xPos + xTrans, 0, zPos + zTrans);
}

float getGrassHeight(float2 coords)
{
	float depth = GetTexture(TexIdTerrainDepth).SampleLevel(g_sampler, coords, 0).r;
	
	float z = depth * 2.0 - 1.0;
    float4 clipSpacePosition = float4(coords * 2.0 - 1.0, z, 1.0);
    float4 viewSpacePosition = mul(InvProjectionMatrix, clipSpacePosition);
    viewSpacePosition /= viewSpacePosition.w;

	return BoundsMax.y - viewSpacePosition.z / 2 - 12;
}

float3 getGrassColor(float2 coords)
{
	return GetTexture(TexIdTerrainColor).SampleLevel(g_sampler, coords, 0).rgb;
}

[numthreads(128, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x * 2;
	
	if (index >= GrassCount) return;

	float3 pos1;
	float3 pos2;
	createGrassPositions(index, pos1, pos2);
	
	float2 coords1 = getGrassCoords(pos1);
	float2 coords2 = getGrassCoords(pos2);
	
	pos1.y = getGrassHeight(coords1);
	pos2.y = getGrassHeight(coords2);
	
	if (abs(pos1.y - pos2.y) < 4)
	{
		uint finalIndex;
		counterBuffer.InterlockedAdd(0, 2, finalIndex);

		grassPos[finalIndex].pos = pos1;
		grassPos[finalIndex + 1].pos = pos2;
		
		grassPos[finalIndex].color = getGrassColor(coords1);
		grassPos[finalIndex + 1].color = getGrassColor(coords2);
	}
}
