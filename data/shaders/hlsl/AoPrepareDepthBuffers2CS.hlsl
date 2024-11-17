//
// Original Author:  James Stanard 
//

cbuffer CB0 : register(b0)
{
	uint ResIdDS8x;
	uint ResIdDS8xAtlas;
	uint ResIdDS16x;
	uint ResIdDS16xAtlas;
}

Texture2D<float> DS4x : register(t0);

[numthreads( 8, 8, 1 )]
void main( uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
    float m1 = DS4x[DTid.xy << 1];

    uint2 st = DTid.xy;
    uint2 stAtlas = st >> 2;
    uint stSlice = (st.x & 3) | ((st.y & 3) << 2);
	
	RWTexture2D<float2> DS8x = ResourceDescriptorHeap[ResIdDS8x];
    DS8x[st] = m1;
	RWTexture2DArray<float> DS8xAtlas = ResourceDescriptorHeap[ResIdDS8xAtlas];
    DS8xAtlas[uint3(stAtlas, stSlice)] = m1;

    if ((GI & 011) == 0)
    {
        uint2 st = DTid.xy >> 1;
        uint2 stAtlas = st >> 2;
        uint stSlice = (st.x & 3) | ((st.y & 3) << 2);
		
		RWTexture2D<float2> DS16x = ResourceDescriptorHeap[ResIdDS16x];
		DS16x[st] = m1;
		RWTexture2DArray<float> DS16xAtlas = ResourceDescriptorHeap[ResIdDS16xAtlas];    
        DS16xAtlas[uint3(stAtlas, stSlice)] = m1;
    }
}
