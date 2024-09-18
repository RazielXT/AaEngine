vertex_shader depth_vs
{
	file depthWrite.hlsl
	entry VSMain
	profile vs_5_0
}

pixel_shader depth_ps
{
	file depthWrite.hlsl
	entry PSMain
	profile ps_5_0
}

vertex_shader depthvox_vs
{
	file depthWriteAndVoxel.hlsl
	entry VSMain
	profile vs_5_0
}

pixel_shader depthvox_ps
{
	file depthWriteAndVoxel.hlsl
	entry PSMain
	profile ps_5_0
}
