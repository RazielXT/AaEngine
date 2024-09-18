vertex_shader vsQuad
{
	file postprocessing.hlsl
	entry VSQuad
	profile vs_5_0
}

pixel_shader psPassLum
{
	file pp_pass.hlsl
	entry PS_MainLum
	profile ps_5_0
}

pixel_shader psBlurX
{
	file blur.hlsl
	entry PS_BlurX
	profile ps_5_0
}

pixel_shader psBlurY
{
	file blur.hlsl
	entry PS_BlurY
	profile ps_5_0
}

pixel_shader psAddThrough
{
	file pp_pass.hlsl
	entry PS_MainAddT
	profile ps_5_0
}