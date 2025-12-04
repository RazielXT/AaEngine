uint width, height;
uint ResIdHeightMap;
uint ResIdWaterMap;


#ifndef DIRECT_LOAD
SamplerState LinearSampler : register(s0);
#endif

struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

RWStructuredBuffer<Vertex> gVertices : register(u0);

#ifdef DIRECT_LOAD
float readHeight(int3 coord)
{
    Texture2D<float> gHeightmap = ResourceDescriptorHeap[ResIdWaterMap];
    Texture2D<float> Terrain = ResourceDescriptorHeap[ResIdHeightMap];
	return max(gHeightmap.Load(coord), Terrain.Load(coord));
}
#else
float readHeight(float2 uv)
{
    Texture2D<float> gHeightmap = ResourceDescriptorHeap[ResIdWaterMap];
    Texture2D<float> Terrain = ResourceDescriptorHeap[ResIdHeightMap];
	float heightValue = gHeightmap.SampleLevel(LinearSampler, uv, 0);
	return max(heightValue, Terrain.SampleLevel(LinearSampler, uv, 0) * 50);
}
#endif

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= width || DTid.y >= height)
        return;

    Texture2D<float> gHeightmap = ResourceDescriptorHeap[ResIdWaterMap];
    Texture2D<float> Terrain = ResourceDescriptorHeap[ResIdHeightMap];

    // --- Height sampling ---
    float heightValue;
#ifdef DIRECT_LOAD
    int3 coord = int3(DTid.xy, 0);
    heightValue = readHeight(coord);
#else
    float2 uv = float2(DTid.x / (float)width, DTid.y / (float)height);
    heightValue = readHeight(uv);
#endif

    uint vertexIndex = DTid.y * width + DTid.x;

    // --- Position ---
    gVertices[vertexIndex].position.y = heightValue;

    // --- UV ---
    gVertices[vertexIndex].uv = float2(
        (float)DTid.x / (width - 1),
        (float)DTid.y / (height - 1));

	float stepWidth = 0.05f;

#ifdef DIRECT_LOAD
    float3 tangent = float3(0.0f, readHeight(coord + int3(1,0,0)) - heightValue, stepWidth);
    float3 binormal = float3(stepWidth, readHeight(coord + int3(0,1,0)) - heightValue, 0.0f);
#else
    float3 tangent = float3(0.0f, readHeight(uv + float2(1 / (float)width, 0)) - heightValue, stepWidth);
    float3 binormal = float3(stepWidth, readHeight(uv + float2(0, 1 / (float)height)) - heightValue, 0.0f);
#endif

    // Calculate normal using cross product
    float3 normal = normalize(cross(tangent, binormal));

    gVertices[vertexIndex].normal = normal;
}
