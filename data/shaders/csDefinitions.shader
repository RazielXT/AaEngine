compute_shader grassInit
{
	file grassInitCS.hlsl
	entry CSMain
	profile cs_6_6
}

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

compute_shader AoPrepareDepthBuffers
{
	file AoPrepareDepthBuffers1CS.hlsl
	entry main
	profile cs_6_6
}

compute_shader AoPrepareDepthBuffers2
{
	file AoPrepareDepthBuffers2CS.hlsl
	entry main
	profile cs_6_6
}

compute_shader AoRenderCS
{
	file AoRenderCS.hlsl
	entry main
	profile cs_6_6
}

compute_shader AoRenderInterleaveCS
{
	file AoRenderCS.hlsl
	entry main
	profile cs_6_6
	defines INTERLEAVE_RESULT
}

compute_shader AoBlurAndUpsampleCS
{
	file AoBlurAndUpsampleCS.hlsl
	entry main
	profile cs_6_6
	defines COMBINE_LOWER_RESOLUTIONS BLEND_WITH_HIGHER_RESOLUTION
}

compute_shader AoBlurAndUpsampleFinalCS
{
	file AoBlurAndUpsampleCS.hlsl
	entry main
	profile cs_6_6
}
