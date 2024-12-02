float4x4 InvProjectionMatrix;
float GrassSpacing;
float3 BoundsMin;
float3 BoundsMax;
float GrassWidth;
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

float getRandom(float x, float z)
{
    float2 K1 = float2(
        23.14069263277926, // e^pi (Gelfond's constant)
         2.665144142690225 // 2^sqrt(2) (Gelfond–Schneider constant)
    );
    return frac(cos( dot(float2(x,z),K1) ) * 12345.6789 );
}

void createGrassPositions(uint index, out float3 pos1, out float3 pos2)
{
	uint x = index / GrassCountRows;
	uint z = index % GrassCountRows;

	float xPos = BoundsMin.x + x * GrassSpacing;
	float zPos = BoundsMin.z + z * GrassSpacing;
	
	float angle = getRandom(xPos, zPos) * 6.28;
	float xTrans = cos(angle) * GrassWidth;
	float zTrans = sin(angle) * GrassWidth;

	zPos += getRandom(zPos, xPos) * GrassWidth;
	xPos += getRandom(xPos, zPos) * GrassWidth;

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

	return BoundsMax.y - viewSpacePosition.z / 2;
}

float3 getGrassColor(float2 coords)
{
	return GetTexture(TexIdTerrainColor).SampleLevel(g_sampler, coords, 0).rgb;
}

[numthreads(128, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
	
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
