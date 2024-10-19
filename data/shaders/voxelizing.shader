vertex_shader voxelizeVS
{
	file voxelizingShader.hlsl
	entry VS_Main
	profile vs_5_0
}

pixel_shader voxelizePS
{
	file voxelizingShader.hlsl
	entry PS_Main
	profile ps_5_0

	cbuffer PerMaterial
	{
		float stepping 0.1
		float stepping2 1
	}
}

pixel_shader voxelizeLightPS
{
	file voxelizingShader.hlsl
	entry PS_Main_Light
	profile ps_5_0
	
	cbuffer PerMaterial
	{
		float lightPower 5
	}
}
