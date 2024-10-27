vertex_shader vsQuad
{
	file postprocess.hlsl
	entry VSQuad
	profile vs_6_6
}

pixel_shader psPassThrough
{
	file postprocess.hlsl
	entry PSPassThrough
	profile ps_6_6
}

pixel_shader psPutBlurThrough
{
	file postprocess.hlsl
	entry PSPutBlurThrough
	profile ps_6_6
}

pixel_shader psAddThrough
{
	file postprocess.hlsl
	entry PSAddThrough
	profile ps_6_6
}

pixel_shader psBlurX
{
	file postprocess.hlsl
	entry PSBlurX
	profile ps_6_6
}

pixel_shader psBlurY
{
	file postprocess.hlsl
	entry PSBlurY
	profile ps_6_6
}

pixel_shader psFXAA
{
	file fxaa.hlsl
	entry PS_FXAA
	profile ps_6_6
}