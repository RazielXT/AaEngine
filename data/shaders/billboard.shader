vertex_shader vsBillboard
{
	file billboard.hlsl
	entry VSMain
	profile vs_6_6
}

pixel_shader psBillboard
{
	file billboard.hlsl
	entry PSMain
	profile ps_6_6
}

pixel_shader psBillboardDepth
{
	file billboard.hlsl
	entry PSMainDepth
	profile ps_6_6
}

vertex_shader vsBillboardEntityId : vsBillboard
{
	defines ENTITY_ID
}

pixel_shader psBillboardEntityId
{
	file billboard.hlsl
	entry PSMainEntityId
	profile ps_6_6
	defines ENTITY_ID
}
