vertex_shader voxelizeVS
{
	file voxelizing.hlsl
	entry VS_Main
	profile vs_6_6
}

vertex_shader voxelizeVSInstancing : voxelizeVS
{
	defines INSTANCED
}

pixel_shader voxelizePS
{
	file voxelizing.hlsl
	entry PS_Main
	profile ps_6_6
}

pixel_shader voxelizePSInstancing : voxelizePS
{
	defines INSTANCED
}

vertex_shader voxelizeVSTerrain : voxelizeVS
{
	defines TERRAIN
}

pixel_shader voxelizePSTerrain : voxelizePS
{
	defines TERRAIN
}
