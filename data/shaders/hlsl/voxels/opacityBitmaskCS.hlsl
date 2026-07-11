Texture3D<float> OpacityGrid : register(t0);
RWTexture3D<uint2> BitmaskGrid : register(u0);

[numthreads(8,8,8)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
	uint mask = 0;

	[unroll]
	for(uint z = 0; z < 2; z++)
	{
		[unroll]
		for(uint y = 0; y < 2; y++)
		{
			[unroll]
			for(uint x = 0; x < 2; x++)
			{
				uint bitIndex = x | (y << 1) | (z << 2);

				float opacity = OpacityGrid.Load(int4(tid * 2 + uint3(x,y,z), 0));

				if(opacity > 0.5f)
					mask |= (1u << bitIndex);
			}
		}
	}

	BitmaskGrid[tid] = mask;
}