// Reconstructs a normal from SNORM-encoded XZ (range -1..1)
float3 DecodeNormalSNORM(float2 xz)
{
	float2 nxz = xz; // already in -1..1 range
	float y = sqrt(saturate(1.0 - dot(nxz, nxz)));
	return float3(nxz.x, y, nxz.y);
}

// Reconstructs a normal from UNORM-encoded XZ (range 0..1)
float3 DecodeNormalUNORM(float2 xz)
{
	float2 nxz = xz * 2.0 - 1.0; // convert to -1..1
	float y = sqrt(saturate(1.0 - dot(nxz, nxz)));
	return float3(nxz.x, y, nxz.y);
}
