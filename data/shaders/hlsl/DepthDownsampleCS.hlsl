cbuffer CB0 : register(b0)
{
	uint ResIdDepthSource;
	uint ResIdDepthResultHigh;
	uint ResIdDepthResult;
}

groupshared float g_Cache1[64];
groupshared float g_Cache2[8];

[numthreads( 8, 8, 1 )]
void main( uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
	//offset for 16x16 grids
	uint2 startST = (Gid.xy << 4) + (GTid.xy * 2);

	Texture2D<float> DepthSource = ResourceDescriptorHeap[ResIdDepthSource];

	float depth = DepthSource[startST + uint2(0, 0)];
	depth = min(depth, DepthSource[startST + uint2(1, 0)]);
	depth = min(depth, DepthSource[startST + uint2(0, 1)]);
	depth = min(depth, DepthSource[startST + uint2(1, 1)]);

	RWTexture2D<float> DepthResultHigh = ResourceDescriptorHeap[ResIdDepthResultHigh];
	DepthResultHigh[startST / 2.0f] = depth;

	//GroupThreadID linear idx
    uint destIdx = (GTid.y * 8) + GTid.x;
    g_Cache1[destIdx] = depth;

    GroupMemoryBarrierWithGroupSync();

	if (GTid.y == 0)
	{
		float depth2 = 1;
		for (uint i = 0; i < 8; i++)
		{
			uint destIdx1 = i * 8 + GTid.x;
			depth2 = min(depth2, g_Cache1[destIdx1]);
		}

		g_Cache2[GTid.x] = depth2;
	}

	GroupMemoryBarrierWithGroupSync();

	if (GTid.y == 0 && GTid.x == 0)
	{
		float depthTotal = 1;
		for (uint i = 0; i < 8; i++)
		{
			depthTotal = min(depthTotal, g_Cache2[i]);
		}

		RWTexture2D<float> DepthResult = ResourceDescriptorHeap[ResIdDepthResult];
		DepthResult[Gid.xy] = depthTotal;
	}
}
