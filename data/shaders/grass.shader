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

vertex_shader vsGrassLeavesEntityId : vsGrassLeaves
{
	defines ENTITY_ID
}

pixel_shader psGrassLeavesEntityId
{
	file grass.hlsl
	entry PSMainEntityId
	profile ps_6_6
	defines ENTITY_ID
}

compute_shader grassInit
{
	file grassInitCS.hlsl
	entry CSMain
	profile cs_6_6
}
