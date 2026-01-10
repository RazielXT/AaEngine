uint SrcMipIndex;
float3 InvOutTexelSize;

SamplerState LinearSampler : register(s0);

Texture3D<float4> inputTexture : register(t0);
RWTexture3D<float4> mipTexture : register(u0);
RWTexture3D<float4> mip2Texture : register(u1);
RWTexture3D<float4> mip3Texture : register(u2);

groupshared float4 GroupValues[4][4][4];

//#define ALL_MAX

[numthreads(4,4,4)]
void CSMain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
#ifdef ALL_MAX
	{
		float4 mipValue = 0;
		for (uint x = 0; x < 2; x++)
			for (uint y = 0; y < 2; y++)
				for (uint z = 0; z < 2; z++)
					mipValue = max(mipValue, inputTexture.Load(uint4(DTid * 2 + uint3(x,y,z), SrcMipIndex)));

		mipTexture[DTid] = mipValue;
		GroupValues[GTid.x][GTid.y][GTid.z] = mipValue;
	}
#else
	float3 texCoord = (float3(DTid) + 0.5f) * InvOutTexelSize;
	float4 srcColor = inputTexture.SampleLevel(LinearSampler, texCoord, SrcMipIndex);

	mipTexture[DTid] = srcColor;

	GroupValues[GTid.x][GTid.y][GTid.z] = srcColor;
#endif

	GroupMemoryBarrierWithGroupSync();

	if (all(GTid % 2 == 0)) 
	{
		float4 mipValue = 0;

		for (uint x = 0; x < 2; x++)
			for (uint y = 0; y < 2; y++)
				for (uint z = 0; z < 2; z++)
					mipValue = max(mipValue, GroupValues[GTid.x+x][GTid.y+y][GTid.z+z]);

		mip2Texture[DTid / 2] = mipValue;
		GroupValues[GTid.x][GTid.y][GTid.z] = mipValue;
	}

	GroupMemoryBarrierWithGroupSync();

	if (all(GTid % 4 == 0)) 
	{
		float4 mipValue = 0;

		for (uint x = 0; x < 4; x+=2)
			for (uint y = 0; y < 4; y+=2)
				for (uint z = 0; z < 4; z+=2)
					mipValue = max(mipValue, GroupValues[GTid.x+x][GTid.y+y][GTid.z+z]);

		mip3Texture[DTid / 4] = mipValue;
	}
}
