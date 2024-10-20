vertex_shader vsTest
{
	file shaderTest.hlsl
	entry VSMain
	profile vs_6_6
}

vertex_shader vsTestVc : vsTest
{
	defines USE_VC
}

vertex_shader vsTestInstancing : vsTest
{
	defines INSTANCED
}

pixel_shader psTest
{
	file shaderTest.hlsl
	entry PSMain
	profile ps_6_6
}

pixel_shader psTestVc : psTest
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
