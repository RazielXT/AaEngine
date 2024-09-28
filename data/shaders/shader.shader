vertex_shader vsTest
{
	file shaderTest.hlsl
	entry VSMain
	profile vs_6_6
}

pixel_shader psTest
{
	file shaderTest.hlsl
	entry PSMain
	profile ps_6_6
}

vertex_shader vsDepthWrite
{
	file depthWrite.hlsl
	entry VSMain
	profile vs_6_6
}
