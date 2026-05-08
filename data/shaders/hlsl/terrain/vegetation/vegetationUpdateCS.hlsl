#include "vegetationCommon.hlsl"

float4 FrustumPlanes[6];
float3 ChunkWorldMin;
float ChunkSize;

StructuredBuffer<SubgroupMeta> subgroupMetaBuffer : register(t0);
RWByteAddressBuffer drawCommandsBuffer : register(u0);
RWStructuredBuffer<uint> redirectBuffer : register(u1);

groupshared bool g_visible;
groupshared uint g_baseOffset;

bool isSubgroupVisible(float3 center, float3 extents)
{
	[unroll]
	for (uint i = 0; i < 5; i++) // skip far plane
	{
		float r = dot(abs(FrustumPlanes[i].xyz), extents);
		float d = dot(FrustumPlanes[i].xyz, center) + FrustumPlanes[i].w;
		if (d < -r)
			return false;
	}
	return true;
}

[numthreads(64, 1, 1)]
void main(uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	uint subgroupIdx = groupID.y * SubgroupsPerDim + groupID.x;

	SubgroupMeta meta = subgroupMetaBuffer[subgroupIdx];
	uint count = min((uint)max(meta.counter, 0), MaxItemsPerSubgroup);

	// First thread checks visibility and reserves draw instances
	if (groupIndex == 0)
	{
		g_visible = false;

		if (count > 0)
		{
			uint2 subgroupXY = uint2(subgroupIdx % SubgroupsPerDim, subgroupIdx / SubgroupsPerDim);
			float subgroupSize = ChunkSize / SubgroupsPerDim;

			float minX = ChunkWorldMin.x + subgroupXY.x * subgroupSize;
			float maxX = minX + subgroupSize;
			float minZ = ChunkWorldMin.z + subgroupXY.y * subgroupSize;
			float maxZ = minZ + subgroupSize;

			float minY = float(meta.minY) / 100.0;
			float maxY = float(meta.maxY) / 100.0;

			float3 center = float3((minX + maxX) * 0.5, (minY + maxY) * 0.5, (minZ + maxZ) * 0.5);
			float3 extents = float3((maxX - minX) * 0.5, (maxY - minY) * 0.5 + 50.0, (maxZ - minZ) * 0.5);

			if (isSubgroupVisible(center, extents))
			{
				g_visible = true;
				drawCommandsBuffer.InterlockedAdd(4, count, g_baseOffset);
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if (!g_visible)
		return;

	// Fill redirect buffer: 64 threads spread across items
	uint baseOffset = g_baseOffset;
	uint infoBase = subgroupIdx * MaxItemsPerSubgroup;

	for (uint i = groupIndex; i < count; i += 64)
	{
		if (!WaveActiveAnyTrue(i < count))
			break;

		redirectBuffer[baseOffset + i] = infoBase + i;
	}
}
