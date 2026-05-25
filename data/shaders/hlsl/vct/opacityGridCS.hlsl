Texture3D<float> SourceOccupancyMip0 : register(t0);
RWTexture3D<uint> OutputDistanceMap : register(u0);

[numthreads(4, 4, 4)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint3 gridDimensions;
    SourceOccupancyMip0.GetDimensions(gridDimensions.x, gridDimensions.y, gridDimensions.z);
    if (any(DTid >= gridDimensions)) return;

    float baseOccupancy = SourceOccupancyMip0.Load(int4(DTid, 0)).r;
    if (baseOccupancy > 0.0f)
    {
        OutputDistanceMap[DTid] = 1u; // Occupied
        return;
    }

    // Search a local bounding box around the voxel to find the nearest solid hit
    int searchRadius = 7; // Gives a max safe skip of 7 voxels perfectly calculated
    int minDistSq = 10000;

    [loop]
    for (int z = -searchRadius; z <= searchRadius; z++)
    {
        for (int y = -searchRadius; y <= searchRadius; y++)
        {
            for (int x = -searchRadius; x <= searchRadius; x++)
            {
                int3 samplePos = int3(DTid) + int3(x, y, z);
                
                // Keep inside grid bounds
                if (any(samplePos < 0) || any(samplePos >= (int3)gridDimensions)) continue;

                float occ = SourceOccupancyMip0.Load(int4(samplePos, 0)).r;
                if (occ > 0.0f)
                {
                    int distSq = x*x + y*y + z*z;
                    minDistSq = min(minDistSq, distSq);
                }
            }
        }
    }

    // Convert the squared distance back to a linear conservative voxel step
    uint safeDistanceVoxels = (uint)floor(sqrt((float)minDistSq));
    
    // Clamp to make sure we don't return 0 for empty spaces
    safeDistanceVoxels = max(1u, safeDistanceVoxels); 


// NEW: Calculate exact distance to the 6 borders of the chunk
    uint distToMinX = DTid.x;
    uint distToMinY = DTid.y;
    uint distToMinZ = DTid.z;
    
    uint distToMaxX = (gridDimensions.x - 1u) - DTid.x;
    uint distToMaxY = (gridDimensions.y - 1u) - DTid.y;
    uint distToMaxZ = (gridDimensions.z - 1u) - DTid.z;

    // Find the absolute closest border
    uint borderLimitX = min(distToMinX, distToMaxX);
    uint borderLimitY = min(distToMinY, distToMaxY);
    uint borderLimitZ = min(distToMinZ, distToMaxZ);
    uint absoluteBorderLimit = min(borderLimitX, min(borderLimitY, borderLimitZ));

    // Dim the borders! 
    // We cannot guarantee safety past the grid edge, so clamp it.
    // We add 1 because if you are exactly on the border block, you can safely step 1 voxel out.
    safeDistanceVoxels = min(safeDistanceVoxels, absoluteBorderLimit + 1u);

    // Guard rails: ensure it's still at least 1
    safeDistanceVoxels = max(1u, safeDistanceVoxels);

    // Encode: Bit 0 = 0 (empty), Bits 1-7 = distance
    uint encodedValue = (safeDistanceVoxels << 1) | 0u;
    OutputDistanceMap[DTid] = encodedValue;
}
