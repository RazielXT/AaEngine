cbuffer InteractionParams : register(b0)
{
	uint queryCount;
	uint TexIdWaterHeight;
	uint TexIdWaterVelocity;
	uint padding;
	float2 worldCenter;
	float2 worldSize;
	float waterHeight;
	float waterHeightStart;
};

struct InteractionInput
{
	float3 worldPosition;
	float padding;
};

struct InteractionOutput
{
	float waterHeightDiff; // positive = submerged below water surface
	float2 waterVelocity;  // local water velocity at position
	float padding;
};

StructuredBuffer<InteractionInput> inputPositions : register(t0);
RWStructuredBuffer<InteractionOutput> outputResults : register(u0);

SamplerState LinearSampler : register(s0);

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	uint idx = DTid.x;
	if (idx >= queryCount)
		return;

	float3 worldPos = inputPositions[idx].worldPosition;

	// Convert world position to UV coordinates in water texture
	float2 uv = (worldPos.xz - worldCenter) / worldSize;

	InteractionOutput result;
	result.waterHeightDiff = 0;
	result.waterVelocity = float2(0, 0);
	result.padding = 0;

	// Check bounds
	if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
	{
		outputResults[idx] = result;
		return;
	}

	Texture2D<float> WaterHeight = ResourceDescriptorHeap[TexIdWaterHeight];
	Texture2D<float2> WaterVelocity = ResourceDescriptorHeap[TexIdWaterVelocity];

	float waterH = WaterHeight.SampleLevel(LinearSampler, uv, 0) / 50.f;
	float2 velocity = WaterVelocity.SampleLevel(LinearSampler, uv, 0);

	// Scale water height from sim space (0-50) to world space
	float waterHeightWorld = waterH * waterHeight + waterHeightStart;

	// Positive = body center is below water surface (submerged)
	// Body height relative to water surface (negative = above water)
	result.waterHeightDiff = waterHeightWorld - worldPos.y;
	result.waterVelocity = velocity;

	outputResults[idx] = result;
}
