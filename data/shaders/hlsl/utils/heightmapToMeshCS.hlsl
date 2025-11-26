uint width, height;

Texture2D<float> gHeightmap : register(t0);

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

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= width || DTid.y >= height)
        return;

    // --- Height sampling ---
    float heightValue;
#ifdef DIRECT_LOAD
    int3 coord = int3(DTid.xy, 0);
    heightValue = gHeightmap.Load(coord);
#else
    float2 uv = float2(DTid.x / (float)width, DTid.y / (float)height);
    heightValue = gHeightmap.SampleLevel(LinearSampler, uv, 0);
#endif

    uint vertexIndex = DTid.y * width + DTid.x;

    // --- Position ---
    gVertices[vertexIndex].position.y = heightValue;

    // --- UV ---
    gVertices[vertexIndex].uv = float2(
        (float)DTid.x / (width - 1),
        (float)DTid.y / (height - 1));

	float stepWidth = 0.1f;

#ifdef DIRECT_LOAD
    float3 tangent = float3(0.0f, gHeightmap.Load(coord + int3(1,0,0) - heightValue, stepWidth);
    float3 binormal = float3(stepWidth, gHeightmap.Load(coord + int3(0,1,0) - heightValue, 0.0f);
#else
    float3 tangent = float3(0.0f, gHeightmap.SampleLevel(LinearSampler, uv + float2(1 / (float)width, 0), 0) - heightValue, stepWidth);
    float3 binormal = float3(stepWidth, gHeightmap.SampleLevel(LinearSampler, uv + float2(0, 1 / (float)height), 0) - heightValue, 0.0f);
#endif

    // Calculate normal using cross product
    float3 normal = normalize(cross(tangent, binormal));

    gVertices[vertexIndex].normal = normal;
}
