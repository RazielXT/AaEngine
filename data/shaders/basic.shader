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

vertex_shader vsBasicShadingPlainVc : vsBasicShadingVc
{
	defines USE_VC NO_TEXTURE
}

vertex_shader vsBasicShadingPlain : vsBasicShading
{
	defines NO_TEXTURE
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

pixel_shader psBasicShadingPlainVc : psBasicShadingVc
{
	defines USE_VC NO_TEXTURE
}

pixel_shader psBasicShadingPlain : psBasicShading
{
	defines NO_TEXTURE
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

vertex_shader vsDepthWriteAlphaTest : vsDepthWrite
{
	defines ALPHA_TEST
}

pixel_shader psDepthWriteAlphaTest
{
	file depthWrite.hlsl
	entry PSMain
	profile ps_6_6
	defines ALPHA_TEST
}
