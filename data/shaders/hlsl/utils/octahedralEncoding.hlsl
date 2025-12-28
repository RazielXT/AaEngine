// Function to decode a float2 packed normal back to float3
float3 DecodeNormalOctahedral(float2 p)
{
    // 1. Calculate the magnitude of the projection onto the XY plane (L1-norm)
    float abs_p_sum = abs(p.x) + abs(p.y);
    
    // 2. Calculate Z component: Z = 1 - L1-norm
    float z = 1.0f - abs_p_sum;

    // 3. Reconstruct the vector components (handling the reflection/back face)
    float3 n = float3(p.x, p.y, z);

    // 4. Handle the sign swap from the reflection step:
    // If Z < 0, then we must restore the sign of the X and Y components.
    // This is mathematically equivalent to:
    if (z < 0.0f)
    {
		n.xy = (1.0f - abs(n.yx)) * select(n.xy >= 0.0f, 1.0f, -1.0f); // CORRECTED
    }
    
    // 5. Final normalization is often required due to minor precision loss:
    return normalize(n);
}

// Function to encode a float3 normal into a float2 (Octahedral)
float2 EncodeNormalOctahedral(float3 n)
{
    // 1. Project N onto the XZ plane (where N.y is 0)
    // The projection plane is determined by the largest component (|N.x| + |N.y| + |N.z| = 1 for a unit vector)
    float scale = 1.0f / (abs(n.x) + abs(n.y) + abs(n.z));

    // 2. Project onto the octahedron:
    float2 proj = float2(n.x, n.y) * scale;

    // 3. Handle the 'back' face (when N.z < 0)
    // The trick to keeping the mapping one-to-one is to reflect the projected point:
    if (n.z <= 0.0f)
    {
        // proj = (1.0 - abs(proj.yx)) * (proj.xy >= 0.0 ? 1.0 : -1.0);
        proj.x = (1.0f - abs(proj.y)) * (proj.x >= 0.0f ? 1.0f : -1.0f);
        proj.y = (1.0f - abs(proj.x)) * (proj.y >= 0.0f ? 1.0f : -1.0f);
    }
    
    // Scale and bias to get coordinates in [0, 1] for potential packing into UNORM bytes later,
    // but since you are using float2, we can return the [-1, 1] range:
    return proj.xy; // Output is in the range [-1, 1]
}
