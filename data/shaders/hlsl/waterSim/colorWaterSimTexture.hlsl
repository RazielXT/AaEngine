cbuffer Params : register(b0)
{
	uint2 gridSize;
	uint ResIdHeightMap;
	uint ResIdWaterMap;
	uint ResIdVelocityMap;
	uint ResIdColorMap;
};

[numthreads(8,8,1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gridSize.x || id.y >= gridSize.y) return;

    int2 p  = int2(id.xy);

    Texture2D<float>    Height    = ResourceDescriptorHeap[ResIdWaterMap];
    Texture2D<float>    Terrain = ResourceDescriptorHeap[ResIdHeightMap];
    Texture2D<float2>   Velocity  = ResourceDescriptorHeap[ResIdVelocityMap];
    RWTexture2D<float4>  ColorOut = ResourceDescriptorHeap[ResIdColorMap];

    float waterHeight = Height.Load(int3(p, 0));
    float terrainHeight = Terrain.Load(int3(p, 0)) * 10 + 0.01f;
    float2 waterVelocity = Velocity.Load(int3(p, 0));

	float4 color = float4(0.4,0.7, 0.95, 1); //water

	if (terrainHeight >= waterHeight)
		color.rgb = float3(0.2,0.6,0);
	else
		color.rgb = lerp(color.rgb, float3(waterVelocity,0.9), saturate(length(waterVelocity / 2)));

	//color.rgb = saturate(length(waterVelocity));

    ColorOut[p] = color;
}
