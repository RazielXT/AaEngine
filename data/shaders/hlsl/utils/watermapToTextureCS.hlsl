uint width, height;
uint ResIdWaterMap;
uint ResIdTerrainMap;
float2 CameraPos;

SamplerState LinearSampler : register(s0);

RWTexture2D<float2> WaterNormal : register(u0);
RWTexture2D<float> WaterHeight : register(u1);

float readHeight(float2 uv)
{
    Texture2D<float> Heightmap = ResourceDescriptorHeap[ResIdWaterMap];
	return Heightmap.SampleLevel(LinearSampler, uv, 0);
}

float readHeightPoint(int2 p)
{
    Texture2D<float> Heightmap = ResourceDescriptorHeap[ResIdWaterMap];
	return Heightmap.Load(int3(p, 0));
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

	float heightValue = readHeightPoint(DTid.xy);

	float stepWidth = 0.1f;

	float2 uv = float2(DTid.x / (float)(width - 1), DTid.y / (float)(height - 1));
	//float3 tangent = float3(0.0f, readHeight(uv + float2(2 / (float)(width - 1), 0)) - heightValue, stepWidth);
	//float3 binormal = float3(stepWidth, readHeight(uv + float2(0, 2 / (float)(height - 1))) - heightValue, 0.0f);
	float3 tangent = float3(0.0f, readHeightPoint(DTid.xy + int2(2, 0)) - heightValue, stepWidth);
	float3 binormal = float3(stepWidth, readHeightPoint(DTid.xy + int2(2, 0)) - heightValue, 0.0f);
	float3 normal = normalize(cross(tangent, binormal));

	WaterNormal[DTid.xy] = normal.xz;

	float2 worldPosOffset = { -20, 30 };
	float worldScale = 102.4 / 1024;
	float2 worldPos = worldScale * DTid.xy + worldPosOffset;

	float terrainHeight = readTerrainHeight(uv);
	float cameraLodOffset = 3 * saturate((length(CameraPos - worldPos) - 10) * 0.2);
	WaterHeight[DTid.xy] = (terrainHeight > heightValue) ? heightValue - cameraLodOffset : heightValue;
}
