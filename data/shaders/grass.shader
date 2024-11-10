vertex_shader vsGrassLeaves
{
	file grass.hlsl
	entry VSMain
	profile vs_6_6
}

pixel_shader psGrassLeaves
{
	file grass.hlsl
	entry PSMain
	profile ps_6_6
}

pixel_shader psGrassLeavesDepth
{
	file grass.hlsl
	entry PSMainDepth
	profile ps_6_6
}

compute_shader grassInit
{
	file grassInitCS.hlsl
	entry CSMain
	profile cs_6_6
}
