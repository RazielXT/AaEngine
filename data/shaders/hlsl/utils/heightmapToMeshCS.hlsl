uint width, height;
float heightScale;
uint ResIdHeightMap;

SamplerState LinearSampler : register(s0);

struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

RWStructuredBuffer<Vertex> gVertices : register(u0);

float readHeight(float2 uv)
{
    Texture2D<float> gHeightmap = ResourceDescriptorHeap[ResIdHeightMap];
	return gHeightmap.SampleLevel(LinearSampler, uv, 0) * heightScale;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= width || DTid.y >= height)
        return;

	float2 uv = float2(DTid.x / (float)(width - 1), DTid.y / (float)(height - 1));
    float heightValue = readHeight(uv);

    uint vertexIndex = DTid.y * width + DTid.x;

    // --- Position ---
    gVertices[vertexIndex].position.y = heightValue;

	float stepWidth = 0.05f;
    float3 tangent = float3(0.0f, readHeight(uv + float2(1 / (float)width, 0)) - heightValue, stepWidth);
    float3 binormal = float3(stepWidth, readHeight(uv + float2(0, 1 / (float)height)) - heightValue, 0.0f);
    float3 normal = normalize(cross(tangent, binormal));

    gVertices[vertexIndex].normal = normal;
}
