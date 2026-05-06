#include "grassCommon.hlsl"
#include "hlsl/common/DataPacking.hlsl"
#include "hlsl/common/Random.hlsl"

float4 FrustumPlanes[6];
float3 CameraPosition;
float MaxDistance;

RWStructuredBuffer<RenderGrassInfo> transformBuffer : register(u0);
RWByteAddressBuffer drawCommandsBuffer : register(u1);

StructuredBuffer<GrassInfo> infoBuffer : register(t0);
ByteAddressBuffer infoCounterBuffer : register(t1);

bool isVisible(float3 position, float radius, float scale)
{
#ifdef FRUSTUM_CULLING
	[unroll]
	for (uint i = 0; i < 5; i++) //skip far plane
	{
		if (dot(FrustumPlanes[i].xyz, position) + FrustumPlanes[i].w < -radius)
			return false;
	}
#endif

/*#ifdef DISTANCE_CULLING
	float dist = distance(position, CameraPosition);
	if (dist > MaxDistance)
		return false;
#endif*/

	float distSq = dot(position - CameraPosition, position - CameraPosition);
	float rnd = RandomFrom2D(position.xz) * scale;

	const float MinDistanceRemoval = 100;
	const float MaxDistanceRemoval = 600;

	// pick a random cutoff distance
	float cutoff = MinDistanceRemoval + rnd * (MaxDistanceRemoval - MinDistanceRemoval);

	// compare squared vs squared
	float cutoffSq = cutoff * cutoff;

	if (distSq > cutoffSq)
		return false;

	return true;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	const float Radius = 5;
	uint totalCount = infoCounterBuffer.Load(0);

	for (uint i = 0; i < 64; i++)
	{
		uint x = (groupID.x * 64 + i) * 64 + groupIndex; 
		bool inBounds = (x < totalCount);

	   if (!WaveActiveAnyTrue(inBounds))
			break;

		bool visible = false;
		RenderGrassInfo renderInfo;

		if (inBounds)
		{
			GrassInfo info = infoBuffer[x];

			float3 testPos = info.position + float3(0, Radius, 0);
			visible = isVisible(testPos, Radius, info.scale);

			if (visible)
			{
				renderInfo.position = info.position;
				/*float2 rotationScale = UnpackR16G16_FLOAT(info.rotationScale);*/
				renderInfo.scale    = info.scale;
				renderInfo.cosYaw   = cos(info.rotation);
				renderInfo.sinYaw   = sin(info.rotation);
			}
		}

		// If nobody in the wave is visible this iteration, skip to next block
		if (!WaveActiveAnyTrue(visible))
			continue;

		// Count how many visible lanes in this wave
		uint waveVisibleCount = WaveActiveCountBits(visible);

		// One atomic per wave to reserve a contiguous block
		uint baseIndex = 0;
		if (WaveIsFirstLane())
		{
			drawCommandsBuffer.InterlockedAdd(4, waveVisibleCount, baseIndex);
		}

		// Broadcast baseIndex to all lanes
		baseIndex = WaveReadLaneFirst(baseIndex);
		// Per-lane offset among visible lanes
		uint laneOffset = WavePrefixCountBits(visible);

		if (visible)
		{
			transformBuffer[baseIndex + laneOffset] = renderInfo;
		}
	}
}
