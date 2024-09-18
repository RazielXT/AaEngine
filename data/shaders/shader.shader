vertex_shader vsTest
{
	file sampleShader.hlsl
	entry VS_Main
	profile vs_5_0
}

pixel_shader psTest
{
	file sampleShader.hlsl
	entry PS_Main
	profile ps_5_0

	cbuffer PerMaterial
	{
		float4 col 1 0 0 1
	}
}