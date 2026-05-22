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
    uint linearBitIdx =
        groupThreadID.z * 16 +
        groupThreadID.y * 4 +
        groupThreadID.x;

    if (linearBitIdx == 0)
    {
        gsMaskX = 0;
        gsMaskY = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    bool occupied =
        OpacityGrid.Load(int4(dispatchThreadID, 0)) > 0.0f;

    uint bitX = 0;
    uint bitY = 0;

    if (occupied)
    {
        if (linearBitIdx < 32)
            bitX = 1u << linearBitIdx;
        else
            bitY = 1u << (linearBitIdx - 32);
    }

    // Fast wave reduction first
    uint waveMaskX = WaveActiveBitOr(bitX);
    uint waveMaskY = WaveActiveBitOr(bitY);

    // One thread per wave commits
    if (WaveIsFirstLane())
    {
        InterlockedOr(gsMaskX, waveMaskX);
        InterlockedOr(gsMaskY, waveMaskY);
    }

    GroupMemoryBarrierWithGroupSync();

    if (linearBitIdx == 0)
    {
        BitmaskGrid[groupID] = uint2(gsMaskX, gsMaskY);
    }
}