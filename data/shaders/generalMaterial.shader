vertex_shader vsGeneralMaterialWithVoxel
{
	file generalMaterial.hlsl
	entry VS_Main
	profile vs_5_0
}

pixel_shader psGeneralMaterialWithVoxel
{
	file generalMaterial.hlsl
	entry PS_Main
	profile ps_5_0
}

vertex_shader vsGeneralMaterialVC : vsGeneralMaterialWithVoxel
{
	defines USE_VC
}

pixel_shader psGeneralMaterialVC : psGeneralMaterialWithVoxel
{
	defines USE_VC
}
