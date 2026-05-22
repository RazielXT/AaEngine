Texture3D<float> OpacityGrid : register(t0);
RWTexture3D<uint2> BitmaskGrid : register(u0);

groupshared uint gsMaskX;
groupshared uint gsMaskY;

[numthreads(4,4,4)]
void CSMain(
	uint3 groupID          : SV_GroupID,
	uint3 groupThreadID    : SV_GroupThreadID,
	uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint linearBitIdx = groupThreadID.z * 16 + groupThreadID.y * 4 + groupThreadID.x;

	// Single thread clears the shared storage
	if (linearBitIdx == 0u)
	{
		gsMaskX = 0u;
		gsMaskY = 0u;
	}

	GroupMemoryBarrierWithGroupSync();

	float opacity = OpacityGrid.Load(int4(dispatchThreadID, 0));
	bool occupied = opacity > 0.0f;

	// 1. Accumulate our bit into a local REGISTER first (completely free/independent)
	uint localBitX = 0u;
	uint localBitY = 0u;

	if (occupied)
	{
		if (linearBitIdx < 32u)
			localBitX = 1u << linearBitIdx;
		else
			localBitY = 1u << (linearBitIdx - 32u); // Explicitly unsigned suffix
	}

	// 2. ONLY execute atomics if this specific thread actually has a bit to append.
	// In empty spaces, threads skip this entirely. In full spaces, we drastically 
	// reduce atomic traffic.
	if (localBitX != 0u)
	{
		InterlockedOr(gsMaskX, localBitX);
	}
	
	if (localBitY != 0u)
	{
		InterlockedOr(gsMaskY, localBitY);
	}

	GroupMemoryBarrierWithGroupSync();

	// 3. Output to the 32x32x32 global grid
	if (linearBitIdx == 0u)
	{
		BitmaskGrid[groupID] = uint2(gsMaskX, gsMaskY);
	}
}