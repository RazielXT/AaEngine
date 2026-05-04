cbuffer AdjustParams : register(b0)
{
	uint2 gridSize;
	float2 center;     // texel-space center of adjustment
	float radius;      // radius in texels
	float heightDelta; // positive = add water, negative = remove
	uint TexIdWaterMap;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= gridSize.x || DTid.y >= gridSize.y)
		return;

	float2 pos = float2(DTid.xy);
	float dist = length(pos - center);

	if (dist >= radius)
		return;

	float falloff = smoothstep(0.0, radius, dist);
	float delta = heightDelta * 10 * (1.0 - falloff * falloff);

	RWTexture2D<float> WaterMap = ResourceDescriptorHeap[TexIdWaterMap];

	WaterMap[DTid.xy] += delta;
}
