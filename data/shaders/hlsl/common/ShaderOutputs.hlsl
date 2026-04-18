#pragma once

// GBuffer pixel shader output - written by all material/object passes.
struct GBufferOutput
{
	float4 albedo        : SV_Target0;
	float4 normals       : SV_Target1;
	float4 motionVectors : SV_Target2;
};

// Entity-id query pixel shader output - written by id-pass material variants.
struct EntityIdOutput
{
	uint4  id       : SV_Target0;
	float4 position : SV_Target1;
	float4 normal   : SV_Target2;
};
