// Reconstructs a normal from SNORM-encoded XY (range -1..1)
float3 DecodeNormalSNORM(float2 xy)
{
    float2 nxy = xy; // already in -1..1 range
    float z = sqrt(saturate(1.0 - dot(nxy, nxy)));
    return float3(nxy, z);
}

// Reconstructs a normal from UNORM-encoded XY (range 0..1)
float3 DecodeNormalUNORM(float2 xy)
{
    float2 nxy = xy * 2.0 - 1.0; // convert to -1..1
    float z = sqrt(saturate(1.0 - dot(nxy, nxy)));
    return float3(nxy, z);
}
