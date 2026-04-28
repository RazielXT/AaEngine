compute_shader generate3DMipmaps3x
{
	file utils/generate3DMips3xCS.hlsl
	entry CSMain
	profile cs_6_6
}

compute_shader generateXYMips4xCS
{
	file utils/generateXYMips4xCS.hlsl
	entry CSMain
	profile cs_6_6
}

compute_shader AoPrepareDepthBuffers
{
	file postprocess/ssao/AoPrepareDepthBuffers1CS.hlsl
	entry main
	profile cs_6_6
}

compute_shader AoPrepareDepthBuffers2
{
	file postprocess/ssao/AoPrepareDepthBuffers2CS.hlsl
	entry main
	profile cs_6_6
}

compute_shader AoRenderCS
{
	file postprocess/ssao/AoRenderCS.hlsl
	entry main
	profile cs_6_6
}

compute_shader AoRenderInterleaveCS
{
	file postprocess/ssao/AoRenderCS.hlsl
	entry main
	profile cs_6_6
	defines INTERLEAVE_RESULT
}

compute_shader AoBlurAndUpsampleCS
{
	file postprocess/ssao/AoBlurAndUpsampleCS.hlsl
	entry main
	profile cs_6_6
	defines COMBINE_LOWER_RESOLUTIONS BLEND_WITH_HIGHER_RESOLUTION
}

compute_shader AoBlurAndUpsampleFinalCS
{
	file postprocess/ssao/AoBlurAndUpsampleCS.hlsl
	entry main
	profile cs_6_6
}
