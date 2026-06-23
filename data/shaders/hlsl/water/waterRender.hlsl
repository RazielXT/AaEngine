//#define GRID_PADDING

#include "hlsl/vct/iso/VoxelConeTracingCommon.hlsl"
#include "hlsl/postprocess/WorldReconstruction.hlsl"
#include "hlsl/grid/heightmapGridReconstruction.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/ShaderOutputs.hlsl"
#include "hlsl/sky/SkyParams.hlsl"
#include "hlsl/common/NormalDecoding.hlsl"
#include "hlsl/common/Srgb.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 InvViewProjectionMatrix;
float3 WorldPosition;
float3 CameraPosition;
float WaterFade;
float3 WaterColor;

float Time;
float2 ViewportSizeInverse;
uint TexIdHeightmap;
uint TexIdFlowmap;
uint TexIdNormal;
uint TexIdSceneDepthHigh;
uint TexIdCaustics;
uint TexIdMeshNormal;
float2 GridHeightWidth;
uint GridTilesWidth;

#ifdef ENTITY_ID
uint EntityId;
#endif

cbuffer SceneVoxelInfo : register(b1)
{
	SceneVoxelCbuffer VoxelInfo;
};

cbuffer SkyParamsBuffer : register(b2)
{
	SkyParams Sky;
}

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
	p.tilesWidth = GridTilesWidth;
	p.tileResolution = 33;

	GridVertexInfo info = ReadGridVertexInfo(InstancingBuffer[instanceID], vertexID, ResourceDescriptorHeap[TexIdHeightmap], LinearSampler, p);


	const float2 NormalUv = info.uv * 30 - Time * 0.03;
	const float2 NormalUv2 = info.uv * 33 + Time * 0.031;
	float3 normalTex = 0.5 * (GetTexture2D(TexIdNormal).SampleLevel(LinearWrapSampler, NormalUv, 0).rgr * 2.0f - 1.0f);
	normalTex += 0.5 * (GetTexture2D(TexIdNormal).SampleLevel(LinearWrapSampler, NormalUv2, 0).rgr * 2.0f - 1.0f);
	normalTex.z  = sqrt(saturate(1.0f - dot(normalTex.xy, normalTex.xy)));
	normalTex = lerp(normalTex, float3(0, 0, 1), 0.6);
	//info.position.y -= normalTex.r * 10;

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
	//float caustics : SV_Target2;
};

float3 BlendUDN(float3 n1, float3 n2)
{
	return normalize(float3(n1.xy + n2.xy, n1.z * n2.z));
}

PSOutput PSMain(PSInput input)
{
	float2 ScreenUV = input.position.xy * ViewportSizeInverse;

	Texture2D DepthBufferHigh = GetTexture2D(TexIdSceneDepthHigh);
	float groundZ = DepthBufferHigh.Sample(LinearWrapSampler, ScreenUV).r;
	float3 groundPosition = ReconstructWorldPosition(ScreenUV, groundZ, InvViewProjectionMatrix);
	float groundDistance = length(input.worldPosition.xyz - groundPosition);

	//float3 voxelUV = (input.worldPosition.xyz-VoxelInfo.FarVoxels.Offset)/VoxelInfo.FarVoxels.WorldSize;
	//Texture3D voxelmap = ResourceDescriptorHeap[VoxelInfo.FarVoxels.TexId];
	//albedo.rgb *= max(0.1, (voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 3).rgb + voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 4).rgb * 2) * 10);

	float2 flow = GetTexture2D(TexIdFlowmap).Sample(LinearWrapSampler, input.uv).xy;

	const float2 NormalUv = input.uv * 30 - Time * 0.02 + flow * 1.1;
	const float2 NormalUv2 = input.uv.yx * 36 + Time * 0.023 + flow;
	float2 normalTex1 = GetTexture2D(TexIdNormal).Sample(LinearWrapSampler, NormalUv).rg;
	float2 normalTex2 = GetTexture2D(TexIdNormal).Sample(LinearWrapSampler, NormalUv2).rg;

	float2 normalAvg = lerp(normalTex1, normalTex2, 0.5);
	float3 normalTex = DecodeNormalTexture(normalAvg);

	float3 normal = ReadGridNormal(ResourceDescriptorHeap[TexIdMeshNormal], LinearSampler, input.uv);
	//normal = lerp(normal, normalize(normalTex), 0.08);
	normal = BlendUDN(normal, normalTex);

	const float FadeDistance = WaterFade + 0.0001f;
	float fade = groundDistance / FadeDistance;

	float4 albedo;
	albedo.a = saturate(normal.r * 0.2 + fade);

	float lighting = abs(dot(-Sky.SunDirection, normal)) * 0.5f + 0.5f;
	albedo.rgb = SrgbToLinear(WaterColor) * lighting * Sky.SunColor;

	PSOutput output;
	output.color = albedo;
	output.normal = normal.xy;

	/*float2 projSpeed = float2(1,1);
	float2 projOffset = Time * projSpeed * 10 / 8.f;
	float projScale = 0.05f;
	float projection = GetTexture2D(TexIdCaustics).Sample(LinearWrapSampler, (groundPosition.xy+groundPosition.z + projOffset)*projScale).r;
	projection *= GetTexture2D(TexIdCaustics).Sample(LinearWrapSampler, (groundPosition.xy+groundPosition.z + projOffset*-1.1)*projScale * 1.1).r;
	output.caustics = projection * 0.2 * (input.worldPosition.y - groundPosition.y) / 10;*/
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
