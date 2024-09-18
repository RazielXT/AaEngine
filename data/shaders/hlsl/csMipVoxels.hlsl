[numthreads(1, 1, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID,
			Texture3D cMap : register(t0),
			RWTexture3D<float4> mipMap1 : register(u0),
			RWTexture3D<float4> mipMap2 : register(u1),
			RWTexture3D<float4> mipMap3 : register(u2))
{

    float3 blockID = threadID * 8;
    float4 mip1Vals[4][4][4];
	
    for (int x = 0; x < 4; x++)
        for (int y = 0; y < 4; y++)
            for (int z = 0; z < 4; z++)
            {
                float3 thisID = blockID + float3(x, y, z) * 2;
                float4 acc = 0;
		
                for (int ix = 0; ix < 2; ix++)
                    for (int iy = 0; iy < 2; iy++)
                        for (int iz = 0; iz < 2; iz++)
                            acc += cMap.Load(float4(thisID + float3(ix, iy, iz), 0));
		
                acc /= 8;
		
                mip1Vals[x][y][z] = acc;
                mipMap1[thisID] = acc;
            }
	
}