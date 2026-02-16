cbuffer Params : register(b0)
{
	uint ResIdHeightMap;
	uint ResIdNormalMap;
	float HeightScale;
	float UnitSize;
};

groupshared float sharedHeights[10][10];
groupshared float2 sharedNormals[10][10];

SamplerState LinearSampler : register(s0);

float LoadHeight(uint2 p)
{
	Texture2D<float> HeightMap = ResourceDescriptorHeap[ResIdHeightMap];
	//p = p % 1024;
	//return HeightMap.Load(int3(p, 0)) * HeightScale;
	return HeightMap.SampleLevel(LinearSampler, p / 1024.f, 0);
}

[numthreads(8,8,1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 groupThreadId : SV_GroupThreadID)
{
	uint2 p = uint2(id.xy);

	RWTexture2D<float2> NormalMap = ResourceDescriptorHeap[ResIdNormalMap];

	// Calculate shared memory indices
	uint sharedX = groupThreadId.x + 1;
	uint sharedY = groupThreadId.y + 1;;

	// Sample height and store in shared memory
	float height = LoadHeight(p);
	sharedHeights[sharedX][sharedY] = height;

	// Load neighboring heights for border threads
	if (sharedX == 8)
		sharedHeights[9][sharedY] = LoadHeight(uint2(p.x + 1, p.y));
	if (sharedY == 8)
		sharedHeights[sharedX][9] = LoadHeight(uint2(p.x, p.y + 1));
	//if (sharedX == 1)
	//	sharedHeights[0][sharedY] = LoadHeight(uint2(p.x - 1, p.y));
	//if (sharedY == 1)
	//	sharedHeights[sharedX][0] = LoadHeight(uint2(p.x, p.y - 1));

	GroupMemoryBarrierWithGroupSync();

	// Calculate tangents and binormals
	float3 tangent = float3(0.0f, sharedHeights[sharedX + 1][sharedY] - height, 0.5f * UnitSize / HeightScale);
	float3 binormal = float3(0.5f * UnitSize / HeightScale, sharedHeights[sharedX][sharedY + 1] - height, 0.0f);
	//float3 tangent = float3(0.0f, sharedHeights[sharedX + 1][sharedY] - sharedHeights[sharedX - 1][sharedY], UnitSize * 2);
	//float3 binormal = float3(UnitSize * 2, sharedHeights[sharedX][sharedY + 1] - sharedHeights[sharedX][sharedY - 1], 0.0f);
	float3 normal = normalize(cross(tangent, binormal));

	NormalMap[p] = normal.zx;
}
