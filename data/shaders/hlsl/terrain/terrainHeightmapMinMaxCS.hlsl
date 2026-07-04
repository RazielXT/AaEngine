#include "hlsl/grid/heightmapGridSampling.hlsl"

Texture2D<float> InputTexture : register(t0);
RWTexture2D<float2> OutputTexture : register(u0);

SamplerState PointSampler : register(s0);

float2 InvSrcDimensions;

// We only need enough slots for the maximum number of waves in a group.
// For an 8x8 group (64 threads):
// - On NV (32-lane warps): 2 waves.
// - On AMD (64-lane waves): 1 wave.
// Allocating 2 slots safely covers both.
groupshared float2 s_WaveMinMax[2];

[numthreads(8, 8, 1)]
void CSMain(
	uint3 groupID          : SV_GroupID,
	uint3 groupThreadID    : SV_GroupThreadID,
	uint  groupIndex       : SV_GroupIndex)
{
	// 1. Fetch the value
	uint2 pixelCoord = groupID.xy * 8 + groupThreadID.xy;
	// UV = (PixelCoord + 0.5) * (1.0 / Dimensions)
	float2 uv = (float2(pixelCoord) + 0.5f) * InvSrcDimensions;
#ifdef GRID_PADDING
	uv = RemapGridTextureUV(uv);
#endif
	float texel = InputTexture.SampleLevel(PointSampler, uv, 0).r;

	// 2. Wave Intrinsics reduction (No shared memory, purely register-based)
	float waveMin = WaveActiveMin(texel);
	float waveMax = WaveActiveMax(texel);

	// Get wave topology info
	uint laneIndex = WaveGetLaneIndex();
	uint waveIndex = groupIndex / WaveGetLaneCount(); 

	// 3. First lane of each wave writes its result to groupshared memory
	if (WaveIsFirstLane())
	{
		s_WaveMinMax[waveIndex] = float2(waveMin, waveMax);
	}

	// Synchronize ONLY the waves within this thread group
	GroupMemoryBarrierWithGroupSync();

	// 4. The very first thread of the group aggregates the wave results
	if (groupIndex == 0)
	{
		uint numWaves = float(64) / float(WaveGetLaneCount());
		
		float finalMin = s_WaveMinMax[0].x;
		float finalMax = s_WaveMinMax[0].y;

		// If there is a second wave (e.g., 32-lane hardware), look at slot 1
		if (numWaves > 1)
		{
			finalMin = min(finalMin, s_WaveMinMax[1].x);
			finalMax = max(finalMax, s_WaveMinMax[1].y);
		}

		// Write out the single combined min/max for this 8x8 tile
		OutputTexture[groupID.xy] = float2(finalMin, finalMax);
	}
}