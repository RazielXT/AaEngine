compute_shader generate3DMipmaps3x
{
	file utils/generate3DMips3xCS.hlsl
	entry CSMain
	profile cs_6_6
}

compute_shader generateNormalMipmaps4x
{
	file utils/generateNormalMips4xCS.hlsl
	entry CSMain
	profile cs_6_6
}
