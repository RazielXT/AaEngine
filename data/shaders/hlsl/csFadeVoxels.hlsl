[numthreads(8, 8, 8)]
void CSMain(uint3 threadID : SV_DispatchThreadID,
			Texture3D colorMap : register(t0),
			RWTexture3D<float4> voxelMap : register(u0))
{
    float4 color = colorMap.Load(float4(threadID, 0));
    float4 currentColor = voxelMap[threadID];

	//color = lerp(color, currentColor, 0.99);
    voxelMap[threadID] = color;
}
