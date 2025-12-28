float InvOutTexelSize;
uint TexIdSrc;	// Full res source
uint TexIdDst0;	// Mip 1 (1/2 res)
uint TexIdDst1;	// Mip 2 (1/4 res)
uint TexIdDst2;	// Mip 3 (1/8 res)
uint TexIdDst3;	// Mip 4 (1/16 res)

SamplerState LinearSampler : register(s0);

groupshared float2 gs_mips[16][16];

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint groupIdx : SV_GroupIndex, uint3 gId : SV_GroupThreadID)
{
	Texture2D<float2> SrcTex = ResourceDescriptorHeap[TexIdSrc];

	float2 texCoord = (float2(id.xy * 2) + 0.5f) * InvOutTexelSize;
	// 1. Generate Mip 1 (Every thread processes a 2x2)
	float2 mip1 = SrcTex.SampleLevel(LinearSampler, texCoord, 0);

	RWTexture2D<float2> Out1 = ResourceDescriptorHeap[TexIdDst0];
	Out1[id.xy] = mip1;

	// Store in shared memory for next levels
	gs_mips[gId.y][gId.x] = mip1;
	GroupMemoryBarrierWithGroupSync();

	// 2. Generate Mip 2 (Only 4x4 threads active)
	if ((groupIdx & 0x9) == 0) // Logical check for even rows/cols (0, 2, 4, 6)
	{
		float2 mip2 = (gs_mips[gId.y][gId.x] + gs_mips[gId.y][gId.x+1] + 
						gs_mips[gId.y+1][gId.x] + gs_mips[gId.y+1][gId.x+1]) * 0.25f;

		RWTexture2D<float2> Out2 = ResourceDescriptorHeap[TexIdDst1];
		Out2[id.xy / 2] = mip2;
		gs_mips[gId.y][gId.x] = mip2;
	}
	GroupMemoryBarrierWithGroupSync();

	// 3. Generate Mip 3 (Only 2x2 threads active)
	if ((groupIdx & 0x1B) == 0) // Logical check for (0, 4)
	{
		float2 mip3 = (gs_mips[gId.y][gId.x] + gs_mips[gId.y][gId.x+2] + 
						gs_mips[gId.y+2][gId.x] + gs_mips[gId.y+2][gId.x+2]) * 0.25f;

		RWTexture2D<float2> Out3 = ResourceDescriptorHeap[TexIdDst2];
		Out3[id.xy / 4] = mip3;
		gs_mips[gId.y][gId.x] = mip3;
	}
	GroupMemoryBarrierWithGroupSync();

	// 4. Generate Mip 4 (Only 1 thread active)
	if (groupIdx == 0)
	{
		float2 mip4 = (gs_mips[0][0] + gs_mips[0][4] + 
						gs_mips[4][0] + gs_mips[4][4]) * 0.25f;

		RWTexture2D<float2> Out4 = ResourceDescriptorHeap[TexIdDst3];
		Out4[id.xy / 8] = mip4;
	}
}
