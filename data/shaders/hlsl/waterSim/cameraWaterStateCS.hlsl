cbuffer CameraWaterStateParams : register(b0)
{
	uint TexIdWaterHeight;
	float DeltaTime;
	float2 worldCenter;
	float2 worldSize;
	float3 cameraPosition;
	float waterHeight;
	float waterHeightStart;
	float dryingSpeed;
	uint resetState;
};

struct SceneRenderingStateParams
{
	float Underwater;
	float UnderwaterDepth;
	float2 Padding;
};

RWStructuredBuffer<SceneRenderingStateParams> SceneRenderingState : register(u0);

SamplerState LinearSampler : register(s0);

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	SceneRenderingStateParams state = SceneRenderingState[0];
	float previous = (resetState != 0) ? 0.0f : state.Underwater;

	float nextState = max(0.0f, previous - dryingSpeed * DeltaTime);
	float waterDepth = 0.0f;

	float2 uv = (cameraPosition.xz - worldCenter) / worldSize;
	bool inBounds = all(uv >= 0.0f.xx) && all(uv <= 1.0f.xx);
	if (inBounds)
	{
		Texture2D<float> WaterHeight = ResourceDescriptorHeap[TexIdWaterHeight];
		float waterH = WaterHeight.SampleLevel(LinearSampler, uv, 0) / 50.f;
		float waterHeightWorld = waterH * waterHeight + waterHeightStart;
		if (cameraPosition.y < waterHeightWorld)
		{
			nextState = 1.0f;
			waterDepth = waterHeightWorld - cameraPosition.y;
		}
	}

	state.Underwater = saturate(nextState);
	state.UnderwaterDepth = waterDepth;
	state.Padding = 0.0f.x;
	SceneRenderingState[0] = state;
}