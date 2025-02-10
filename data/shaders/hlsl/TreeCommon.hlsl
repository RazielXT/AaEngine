#ifdef BRANCH_WAVE
float3 getBranchWaveOffset(float time, float3 localPos, float2 uv)
{
	float var = sin(time + (localPos.x + localPos.z) * 4) * (1 - uv.y);
	return var / 3;
}
#endif
