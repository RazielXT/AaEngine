vertex_shader vsTest
{
	file shaderTest.hlsl
	entry VSMain
	profile vs_6_6
}

vertex_shader vsTestInstancing
{
	file shaderTest.hlsl
	entry VSMain
	profile vs_6_6
	defines INSTANCED
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

vertex_shader vsDepthWriteInstancing
{
	file depthWrite.hlsl
	entry VSMain
	profile vs_6_6
	defines INSTANCED
}
