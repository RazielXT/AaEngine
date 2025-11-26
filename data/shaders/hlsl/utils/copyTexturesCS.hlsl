uint width, height;

Texture2D<float4> InputTexture : register(t0);

#ifndef DIRECT_LOAD
SamplerState LinearSampler : register(s0);
#endif

RWTexture2D<float4> OutputTexture : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= width || DTid.y >= height)
		return;

#ifdef DIRECT_LOAD
	int3 coord = int3(DTid.xy, 0);
	OutputTexture[DTid.xy] = InputTexture.Load(coord);
#else
	float2 uv = float2(DTid.x / (float)width, DTid.y / (float)height);
	OutputTexture[DTid.xy] = InputTexture.SampleLevel(LinearSampler, uv, 0);
#endif
}
