#include "grassCommon.hlsl"

// Bitmask of tile ids being regenerated this frame; their instances are dropped so the
// find pass can re-append fresh data, keeping the shared buffer compact with one counter.
uint DirtyMask;

StructuredBuffer<GrassInfo> srcInfoBuffer : register(t0);
ByteAddressBuffer srcCounterBuffer : register(t1);

RWStructuredBuffer<GrassInfo> dstInfoBuffer : register(u0);
RWByteAddressBuffer dstCounterBuffer : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	uint totalCount = srcCounterBuffer.Load(0);

	uint x = (groupID.x * 64 + groupID.y) * 64 + groupIndex;
	bool inBounds = (x < totalCount);

	if (!WaveActiveAnyTrue(inBounds))
		return;

	bool keep = false;
	GrassInfo info;

	if (inBounds)
	{
		info = srcInfoBuffer[x];
		keep = (DirtyMask & (1u << info.tileId)) == 0;
	}

	if (!WaveActiveAnyTrue(keep))
		return;

	uint waveKeepCount = WaveActiveCountBits(keep);

	uint baseIndex = 0;
	if (WaveIsFirstLane())
	{
		dstCounterBuffer.InterlockedAdd(0, waveKeepCount, baseIndex);
	}

	baseIndex = WaveReadLaneFirst(baseIndex);
	uint laneOffset = WavePrefixCountBits(keep);

	if (keep)
	{
		dstInfoBuffer[baseIndex + laneOffset] = info;
	}
}
