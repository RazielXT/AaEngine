uint width, height;
uint ResIdWaterMap;
uint ResIdTerrainMap;

SamplerState LinearSampler : register(s0);

RWTexture2D<float4> WaterInfoMap : register(u0);

float readHeight(float2 uv)
{
    Texture2D<float> Heightmap = ResourceDescriptorHeap[ResIdWaterMap];
	return Heightmap.SampleLevel(LinearSampler, uv, 0);
}

float readTerrainHeight(float2 uv)
{
    Texture2D<float> Heightmap = ResourceDescriptorHeap[ResIdTerrainMap];
	return Heightmap.SampleLevel(LinearSampler, uv, 0) * 50;
}

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

    float2 uv = float2(DTid.x / (float)width, DTid.y / (float)height);
    float heightValue = readHeight(uv);

	float stepWidth = 0.05f;
	float heightOffset = 0;

	if (readTerrainHeight(uv) > heightValue)
		heightOffset -= 0.001f ;

    float3 tangent = float3(0.0f, readHeight(uv + float2(1 / (float)width, 0)) - heightValue, stepWidth);
    float3 binormal = float3(stepWidth, readHeight(uv + float2(0, 1 / (float)height)) - heightValue, 0.0f);
    float3 normal = normalize(cross(tangent, binormal));

	WaterInfoMap[DTid.xy] = float4(heightValue + heightOffset, normal);
}
