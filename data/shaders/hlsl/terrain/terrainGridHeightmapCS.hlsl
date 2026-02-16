RWTexture2D<float> OutHeightmap : register(u0);

int2 GridOffset;
uint TexIdNoise;

SamplerState LinearWrapSampler : register(s0);

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
	Texture2D NoiseTexture = ResourceDescriptorHeap[TexIdNoise];
	float height = NoiseTexture.SampleLevel(LinearWrapSampler, uv, 0).r;
	float height2 = NoiseTexture.SampleLevel(LinearWrapSampler, 0.1 + uv / 8, 0).r;
	float heightDetail = NoiseTexture.SampleLevel(LinearWrapSampler, 0.2 + uv * 3, 0).r;
	float heightDetail2 = NoiseTexture.SampleLevel(LinearWrapSampler, 0.1 + uv * 20, 0).r;
	//float sineWave = (sin(sqrt(uv.x+uv.y) * 5) + 1) * 0.5f;

	height2 = pow(saturate(height2 * 1.3),2);

	//OutHeightmap[DTid.xy] = lerp(heightDetail2 * smoothstep(0.4,1,heightDetail), height2 * smoothstep(0,0.5,saturate(height)), 0.98f) * 1.65f - 0.5f;

	OutHeightmap[DTid.xy] = height;//lerp(heightDetail2 * smoothstep(0.4,1,heightDetail), height2 * smoothstep(0,0.8,saturate(height)), 0.98f) * 1.0;
}
