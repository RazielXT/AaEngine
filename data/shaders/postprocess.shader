vertex_shader vsQuad
{
	file postprocess/postprocess.hlsl
	entry VSQuad
	profile vs_6_6
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
