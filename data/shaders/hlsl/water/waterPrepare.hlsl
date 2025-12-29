#include "../VoxelConeTracingCommon.hlsl"
#include "../postprocess/WorldReconstruction.hlsl"
#include "../utils/normalReconstruction.hlsl"
#include "../grid/heightmapGridReconstruction.hlsl"

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
uint TexIdNormalmap;

#ifdef ENTITY_ID
uint EntityId;
#endif

cbuffer SceneVoxelInfo : register(b1)
{
	SceneVoxelCbuffer VoxelInfo;
};

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
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
	p.heightScale = 1.f;
	p.gridSize = 102.4f;
	p.tilesWidth = 64;
	p.tileResolution = 33;

	GridVertexInfo info = ReadGridVertexInfo(InstancingBuffer[instanceID], vertexID, ResourceDescriptorHeap[TexIdHeightmap], LinearSampler, p);

	PSInput result;
	result.worldPosition = info.position;
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.uv = info.uv;
	return result;
}

float4 GetWaves(float2 uv)
{
	//uv.xy -= Time * 0.025;
	float4 albedo = GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, uv);
	uv.xy -= Time * 0.007;
	albedo += GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, uv);

	return albedo / 2;
}

float2 SampleBlurred(Texture2D<float2> tex, SamplerState samp, float2 uv, float invResHalf)
{
	float2 s1 = tex.Sample(samp, uv + float2( invResHalf, 0));
	float2 s2 = tex.Sample(samp, uv + float2(-invResHalf, 0));
	float2 s3 = tex.Sample(samp, uv + float2(0,  invResHalf));
	float2 s4 = tex.Sample(samp, uv + float2(0, -invResHalf));

	return (s1 + s2 + s3 + s4) * 0.25;
}

float3 ReadNormal(float2 uv)
{
	Texture2D<float2> NormalMap = ResourceDescriptorHeap[TexIdNormalmap];
	float2 normalXZ = SampleBlurred(NormalMap, LinearSampler, uv, 0.5f/1024);//NormalMap.Sample(LinearSampler, uv);

	return DecodeNormalSNORM(normalXZ).xzy;
}

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float caustics : SV_Target2;
};

PSOutput PSMain(PSInput input)
{
	float2 ScreenUV = input.position.xy * ViewportSizeInverse;

	Texture2D DepthBufferHigh = GetTexture(TexIdSceneDepthHigh);
	float groundZ = DepthBufferHigh.Sample(LinearWrapSampler, ScreenUV).r;
	float3 groundPosition = ReconstructWorldPosition(ScreenUV, groundZ, InvViewProjectionMatrix);
	float groundDistance = length(input.worldPosition.xyz - groundPosition);

	const float FadeDistance = 1.5f;
	float fade = groundDistance / FadeDistance;

	float4 albedo = GetWaves(input.uv);
	albedo.a = albedo.r * 0.2 + fade;
	albedo.rgb *= float3(0.2,0.8, 0.6);

	float3 voxelUV = (input.worldPosition.xyz-VoxelInfo.FarVoxels.Offset)/VoxelInfo.FarVoxels.SceneSize;
	Texture3D voxelmap = ResourceDescriptorHeap[VoxelInfo.FarVoxels.TexId];
	albedo.rgb *= max(0.3, (voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 3).rgb + voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 4).rgb * 2) * 10);

	float3 normalTex = 0.5 * (GetTexture(TexIdNormal).Sample(LinearWrapSampler,input.uv).rgr * 2.0f - 1.0f);
	normalTex += 0.5 * (GetTexture(TexIdNormal).Sample(LinearWrapSampler,input.uv * 0.7).rgr * 2.0f - 1.0f);
	normalTex.z  = sqrt(saturate(1.0f - dot(normalTex.xy, normalTex.xy)));

	float3 normal = ReadNormal(input.uv);//lerp(float3(0,1,0), normalize(normalTex), 0.1);

	PSOutput output;
	output.color = albedo;
	output.normal = float4(normal,1);

	float2 projSpeed = float2(-1,-1);
	float2 projOffset = Time*projSpeed * 8;
	float projScale = 0.02;
	float projection = GetTexture(TexIdCaustics).Sample(LinearWrapSampler, (groundPosition.xy+groundPosition.z+projOffset)*projScale).r;
	projection *= GetTexture(TexIdCaustics).Sample(LinearWrapSampler, (groundPosition.xy+groundPosition.z+ float2(0.5,0.5) + projOffset*1.11)*projScale).r;
	output.caustics = projection * (input.worldPosition.y - groundPosition.y) / FadeDistance;

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
	output.normal = float4(ReadNormal(input.uv), 0);
	return output;
}

#endif
