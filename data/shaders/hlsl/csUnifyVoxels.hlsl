[numthreads(8, 8, 8)]
void CSMain(uint3 threadID : SV_DispatchThreadID,
			Texture3D cMap : register(t0),
			Texture3D sMap : register(t1),
			RWTexture3D<float4> voxelMap : register(u0))
{
    voxelMap[threadID] = float4(cMap.Load(float4(threadID, 0)).rgb, sMap.Load(float4(threadID, 0)).x);
}