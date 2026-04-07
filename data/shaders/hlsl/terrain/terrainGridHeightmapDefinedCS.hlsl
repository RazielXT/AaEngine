RWTexture2D<float> OutHeightmap : register(u0);

int2 GridOffset;
uint TexIdNoise;

SamplerState LinearWrapSampler : register(s0);

#ifndef HEIGHT_FUNC
float heightFunc(float2 uv)
{
	Texture2D NoiseTexture = ResourceDescriptorHeap[TexIdNoise];
	return NoiseTexture.SampleLevel(LinearWrapSampler, uv, 0).r;
}
#else
HEIGHT_FUNC
#endif

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	const float GridUnitSize = 32.0f;
	const float GridStepUnits = 30;
	//const float GridStepSize = GridStepUnits * GridUnitSize;

	int2 GridUnitsOffset = (GridOffset * GridStepUnits) - 1;
	float2 gridOffsetUv = GridUnitsOffset / GridUnitSize;

	float2 dispatchUv = DTid.xy / 1024.0f;
	float2 globalUnitPos = gridOffsetUv + dispatchUv;

	float2 uv = globalUnitPos;

	OutHeightmap[DTid.xy] = heightFunc(uv);
}
