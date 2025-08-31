vertex_shader vsQuad
{
	file postprocess/postprocess.hlsl
	entry VSQuad
	profile vs_6_6
}

pixel_shader psPassThrough
{
	file postprocess/postprocess.hlsl
	entry PSPassThrough
	profile ps_6_6
}

pixel_shader psPassThroughIllumination
{
	file postprocess/postprocess.hlsl
	entry PSPassThroughIllumination
	profile ps_6_6
}

pixel_shader psPassThroughAvg
{
	file postprocess/postprocess.hlsl
	entry PSPassThroughAvg
	profile ps_6_6
}

pixel_shader psPutBlurThrough
{
	file postprocess/postprocess.hlsl
	entry PSPutBlurThrough
	profile ps_6_6
}

pixel_shader psAddThrough
{
	file postprocess/postprocess.hlsl
	entry PSAddThrough
	profile ps_6_6
}

pixel_shader psAddThroughNoLuma : psAddThrough
{
	defines NO_LUMA
}

pixel_shader psBlurX
{
	file postprocess/postprocess.hlsl
	entry PSBlurX
	profile ps_6_6
}

pixel_shader psBlurY
{
	file postprocess/postprocess.hlsl
	entry PSBlurY
	profile ps_6_6
}

pixel_shader psDarken
{
	file postprocess/postprocess.hlsl
	entry PSDarken
	profile ps_6_6
}

pixel_shader psFXAA
{
	file fxaa.hlsl
	entry PS_FXAA
	profile ps_6_6
}
