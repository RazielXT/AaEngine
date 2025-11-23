vertex_shader vsWaterLake
{
	file water/waterPrepare.hlsl
	entry VSMain
	profile vs_6_6
}

pixel_shader psWaterLake
{
	file water/waterPrepare.hlsl
	entry PSMain
	profile ps_6_6
}

vertex_shader vsWaterLakeEntityId : vsWaterLake
{
	defines ENTITY_ID
}

pixel_shader psWaterLakeEntityId : psWaterLake
{
	defines ENTITY_ID
	entry PSMainEntityId
}
