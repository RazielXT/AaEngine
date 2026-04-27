#define GRID_PADDING

#include "hlsl/vct/iso/VoxelConeTracingCommon.hlsl"
#include "hlsl/postprocess/WorldReconstruction.hlsl"
#include "hlsl/grid/heightmapGridReconstruction.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/ShaderOutputs.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 InvViewProjectionMatrix;
float3 WorldPosition;
float3 CameraPosition;

float Time;
float2 ViewportSizeInverse;
uint TexIdHeightmap;
uint TexIdDiffuse;
uint TexIdNormal;
uint TexIdSceneDepthHigh;
uint TexIdCaustics;
uint TexIdMeshNormal;
float2 GridHeightWidth;

#ifdef ENTITY_ID
uint EntityId;
#endif

cbuffer SceneVoxelInfo : register(b1)
{
	SceneVoxelCbuffer VoxelInfo;
};

SamplerState LinearWrapSampler : register(s0);
SamplerState DepthSampler : register(s1);
SamplerState PointSampler : register(s2);
SamplerState LinearSampler : register(s3);

struct PSInput
{
	float4 position : SV_POSITION;
	float4 worldPosition : TEXCOORD0;
	float2 uv : TEXCOORD1;
};

StructuredBuffer<GridTileData> InstancingBuffer : register(t0);

PSInput VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	GridRuntimeParams p;
	p.cameraPos = CameraPosition;
	p.worldPos = WorldPosition;
	p.heightScale = GridHeightWidth.x / 50;
	p.gridSize = GridHeightWidth.y;
	p.tilesWidth = 512;
	p.tileResolution = 33;

	GridVertexInfo info = ReadGridVertexInfo(InstancingBuffer[instanceID], vertexID, ResourceDescriptorHeap[TexIdHeightmap], LinearSampler, p);

	const float2 NormalUv = info.uv * 10 + Time * 0.01;
	float heightTexture = GetTexture2D(TexIdDiffuse).SampleLevel(LinearWrapSampler, NormalUv, 0).r;
	info.position.y -= heightTexture * 5;

	PSInput result;
	result.worldPosition = info.position;
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.uv = info.uv;
	return result;
}

struct PSOutput
{
	float4 color : SV_Target0;
	float2 normal : SV_Target1;
	float caustics : SV_Target2;
};

PSOutput PSMain(PSInput input)
{
	float2 ScreenUV = input.position.xy * ViewportSizeInverse;

	Texture2D DepthBufferHigh = GetTexture2D(TexIdSceneDepthHigh);
	float groundZ = DepthBufferHigh.Sample(LinearWrapSampler, ScreenUV).r;
	float3 groundPosition = ReconstructWorldPosition(ScreenUV, groundZ, InvViewProjectionMatrix);
	float groundDistance = length(input.worldPosition.xyz - groundPosition);

	const float FadeDistance = 100;
	float fade = groundDistance / FadeDistance;

	const float2 DetailUv = input.uv * 20 + Time * 0.02;

	float4 albedo = GetTexture2D(TexIdDiffuse).Sample(LinearWrapSampler, DetailUv);
	albedo.a = saturate(albedo.r * 0.2 + fade);
	albedo.rgb = float3(0.1,0.15, 0.24) * 0.1;

	float3 voxelUV = (input.worldPosition.xyz-VoxelInfo.FarVoxels.Offset)/VoxelInfo.FarVoxels.WorldSize;
	Texture3D voxelmap = ResourceDescriptorHeap[VoxelInfo.FarVoxels.TexId];
	albedo.rgb *= max(0.1, (voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 3).rgb + voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 4).rgb * 2) * 10);

	const float2 NormalUv = input.uv * 20 + Time * 0.01;
	const float2 NormalUv2 = input.uv * 23 - Time * 0.015;
	float3 normalTex = 0.5 * (GetTexture2D(TexIdNormal).Sample(LinearWrapSampler, NormalUv).rgr * 2.0f - 1.0f);
	normalTex += 0.5 * (GetTexture2D(TexIdNormal).Sample(LinearWrapSampler, NormalUv2).rgr * 2.0f - 1.0f);
	normalTex.z  = sqrt(saturate(1.0f - dot(normalTex.xy, normalTex.xy)));
	normalTex = lerp(normalTex, float3(0, 0, 1), 0.6);

	float3 normal = ReadGridNormal(ResourceDescriptorHeap[TexIdMeshNormal], LinearSampler, input.uv);
	//normal = lerp(normal, normalize(normalTex), 0.08);

	normal = normalize(float3(normal.xy + normalTex.xy, normal.z*normalTex.z));

	PSOutput output;
	output.color = albedo;
	output.normal = normal.xy;

	float2 projSpeed = float2(1,1);
	float2 projOffset = Time * projSpeed * 10;
	float projScale = 0.01;
	float projection = GetTexture2D(TexIdCaustics).Sample(LinearWrapSampler, (groundPosition.xy+groundPosition.z + projOffset)*projScale).r;
	projection *= GetTexture2D(TexIdCaustics).Sample(LinearWrapSampler, (groundPosition.xy+groundPosition.z + projOffset*-1.1)*projScale * 1.1).r;
	output.caustics = projection * 0.3 * (input.worldPosition.y - groundPosition.y) / FadeDistance;
	//output.caustics = 30*abs(GetTexture(TexIdHeightmap).Sample(LinearWrapSampler, input.uv + 0.005f).r - GetTexture(TexIdHeightmap).Sample(LinearWrapSampler, input.uv).r) * fade;
	return output;
}

#ifdef WIREFRAME

float4 PSMainWireframe(PSInput input) : SV_TARGET
{
	return float4(0.2,0.2,0.5,1);
}

#endif


#ifdef ENTITY_ID

EntityIdOutput PSMainEntityId(PSInput input)
{
	EntityIdOutput output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = input.worldPosition;
	output.normal = float4(ReadGridNormal(ResourceDescriptorHeap[TexIdMeshNormal], LinearSampler, input.uv), 0);
	return output;
}

#endif
