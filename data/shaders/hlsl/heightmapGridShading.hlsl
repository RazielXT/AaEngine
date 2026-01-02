#include "grid/heightmapGridReconstruction.hlsl"

float4x4 ViewProjectionMatrix;
float3 MaterialColor;
float3 CameraPosition;
float3 WorldPosition;
uint TexIdDiffuse;
uint TexIdHeightmap;
uint TexIdNormalmap;

#ifdef ENTITY_ID
uint EntityId;
#endif

//#define DEBUG

StructuredBuffer<GridTileData> InstancingBuffer : register(t0);

SamplerState LinearSampler : register(s0);

struct PSInput
{
	float4 position : SV_POSITION;
	float4 worldPosition : TEXCOORD0;
	float2 uv : TEXCOORD1;
#ifdef DEBUG
	float debugColor : TEXCOORD10;
#endif
};

PSInput VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	GridRuntimeParams p;
	p.cameraPos = CameraPosition;
	p.worldPos = WorldPosition;
	p.heightScale = 50.f;
	p.gridSize = 102.4f;
	p.tilesWidth = 64;
	p.tileResolution = 33;

	GridVertexInfo info = ReadGridVertexInfo(InstancingBuffer[instanceID], vertexID, ResourceDescriptorHeap[TexIdHeightmap], LinearSampler, p);

	PSInput result;
	result.worldPosition = info.position;
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.uv = info.uv;

#ifdef DEBUG
	result.debugColor = d / MorphRange.x;
#endif
	return result;
}

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normals : SV_Target1;
	float4 motionVectors : SV_Target2;
};

PSOutput PSMain(PSInput input)
{
	SamplerState sampler = SamplerDescriptorHeap[0];
	float4 albedoTex = GetTexture(TexIdDiffuse).Sample(sampler, input.uv);
	float3 albedo = albedoTex.rgb * MaterialColor;

	//float3 t = normalize(cross(normal, float3(1,0,0)));
#ifdef DEBUG
	albedo = input.debugColor; 	//float3(input.uv.xy, 0);
#endif

	PSOutput output;
	output.albedo = float4(albedo,1);
	output.normals = float4(ReadGridNormal(ResourceDescriptorHeap[TexIdNormalmap], LinearSampler, input.uv), 1);
	output.motionVectors = float4(0, 0, 0, 0);
	return output;
}

#ifdef ENTITY_ID

struct PSOutputId
{
    uint4 id : SV_Target0;
    float4 position : SV_Target1;
	float4 normal : SV_Target2;
};

PSOutputId PSMainEntityId(PSInput input)
{
	PSOutputId output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = input.worldPosition;
	output.normal = float4(ReadGridNormal(ResourceDescriptorHeap[TexIdNormalmap], LinearSampler, input.uv), 0);
	return output;
}

#endif
