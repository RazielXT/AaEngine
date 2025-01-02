vertex_shader vsBasicShading
{
	file basicShading.hlsl
	entry VSMain
	profile vs_6_6
}

vertex_shader vsBasicShadingVc : vsBasicShading
{
	defines USE_VC
}

vertex_shader vsBasicShadingInstancing : vsBasicShading
{
	defines INSTANCED
}

pixel_shader psBasicShading
{
	file basicShading.hlsl
	entry PSMain
	profile ps_6_6
}

pixel_shader psBasicShadingVc : psBasicShading
{
	defines USE_VC
}

vertex_shader vsDepthWrite
{
	file depthWrite.hlsl
	entry VSMain
	profile vs_6_6
}

vertex_shader vsDepthWriteInstancing : vsDepthWrite
{
	defines INSTANCED
}
