uint width, height;
uint ResIdWaterMap;
uint ResIdTerrainMap;

SamplerState LinearSampler : register(s0);

RWTexture2D<float2> WaterNormal : register(u0);

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

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= width || DTid.y >= height)
		return;

	float2 uv = float2(DTid.x / (float)(width - 1), DTid.y / (float)(height - 1));
	float heightValue = readHeight(uv);

	float stepWidth = 0.05f;
	float heightOffset = 0;

	//if (readTerrainHeight(uv) > heightValue)
	//	heightOffset -= 0.01f ;

	float3 tangent = float3(0.0f, readHeight(uv + float2(1 / (float)width, 0)) - heightValue, stepWidth);
	float3 binormal = float3(stepWidth, readHeight(uv + float2(0, 1 / (float)height)) - heightValue, 0.0f);
	float3 normal = normalize(cross(tangent, binormal));

	WaterNormal[DTid.xy] = normal.xz;
}
