vertex_shader vsVoxelTracing
{
	file voxelTracing.hlsl
	entry VSMain
	profile vs_6_6
}

pixel_shader psVoxelTracing
{
	file voxelTracing.hlsl
	entry PSMain
	profile ps_6_6
}

pixel_shader psVoxelTracingAlpha : psVoxelTracing
{
	defines ALPHA_TEST
}

vertex_shader vsVoxelTracingTerrain
{
	file voxelTracingTerrain.hlsl
	entry VSMain
	profile vs_6_6
}

pixel_shader psVoxelTracingTerrain
{
	file voxelTracingTerrain.hlsl
	entry PSMain
	profile ps_6_6
}
