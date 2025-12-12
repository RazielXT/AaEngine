uint width, height;
uint ResIdWaterMap;

#ifndef DIRECT_LOAD
SamplerState LinearSampler : register(s0);
#endif

struct Vertex
{
    float height;
    float2 normal;
};

RWStructuredBuffer<Vertex> gVertices : register(u0);

#ifdef DIRECT_LOAD
float readHeight(int3 coord)
{
    Texture2D<float> gHeightmap = ResourceDescriptorHeap[ResIdWaterMap];
	return gHeightmap.Load(coord);
}
#else
float readHeight(float2 uv)
{
    Texture2D<float> gHeightmap = ResourceDescriptorHeap[ResIdWaterMap];
	return gHeightmap.SampleLevel(LinearSampler, uv, 0);
}
#endif

// Function to encode a float3 normal into a float2 (Octahedral)
float2 EncodeNormalOctahedral(float3 n)
{
    // 1. Project N onto the XZ plane (where N.y is 0)
    // The projection plane is determined by the largest component (|N.x| + |N.y| + |N.z| = 1 for a unit vector)
    float scale = 1.0f / (abs(n.x) + abs(n.y) + abs(n.z));

    // 2. Project onto the octahedron:
    float2 proj = float2(n.x, n.y) * scale;

    // 3. Handle the 'back' face (when N.z < 0)
    // The trick to keeping the mapping one-to-one is to reflect the projected point:
    if (n.z <= 0.0f)
    {
        // proj = (1.0 - abs(proj.yx)) * (proj.xy >= 0.0 ? 1.0 : -1.0);
        proj.x = (1.0f - abs(proj.y)) * (proj.x >= 0.0f ? 1.0f : -1.0f);
        proj.y = (1.0f - abs(proj.x)) * (proj.y >= 0.0f ? 1.0f : -1.0f);
    }
    
    // Scale and bias to get coordinates in [0, 1] for potential packing into UNORM bytes later,
    // but since you are using float2, we can return the [-1, 1] range:
    return proj.xy; // Output is in the range [-1, 1]
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= width || DTid.y >= height)
        return;

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
    gVertices[vertexIndex].height = heightValue;

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

    gVertices[vertexIndex].normal = EncodeNormalOctahedral(normal);
}
