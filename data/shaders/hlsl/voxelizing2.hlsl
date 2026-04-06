#include "VoxelConeTracingCommon.hlsl"
#include "ShadowsCommon.hlsl"

#ifdef GRID
#define GRID_PADDING
#include "grid/heightmapGridReconstruction.hlsl"
#endif

float4x4 ShadowMatrix;
#ifdef GRID
float3 WorldPosition;
uint TexIdHeightmap;
uint TexIdNormalmap;
float3 CameraPosition;
float2 GridHeightWidth;
#else
float4x4 WorldMatrix;
float3 MaterialColor;
#endif
uint TexIdDiffuse;
uint ShadowMapIdx;
uint VoxelIdx;

cbuffer SceneVoxelInfo : register(b1)
{
	SceneVoxelCbufferIndexed VoxelInfo;
};

cbuffer PSSMShadows : register(b2)
{
	SunParams Sun;
}

RWStructuredBuffer<VoxelSceneData> SceneVoxelData : register(u0);

SamplerState LinearSampler : register(s0);
SamplerState ShadowSampler : register(s1);

struct GS_Input
{
	float3 wp : POSITION;
	float2 uv : TEXCOORD0;
#ifndef GRID
	float3 normal  : NORMAL;
#endif
};

struct PS_Input
{
	float4 pos : SV_POSITION; // voxel projection position
	float4 wp : TEXCOORD1;
	float2 uv : TEXCOORD0;
#ifndef GRID
	float3 normal : NORMAL;
#endif
};

#ifdef GRID
StructuredBuffer<GridTileData> InstancingBuffer : register(t0);

GS_Input VS_Main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	GridRuntimeParams p;
	p.cameraPos = CameraPosition;
	p.worldPos = WorldPosition;
	p.heightScale = GridHeightWidth.x;
	p.gridSize = GridHeightWidth.y;
	p.tilesWidth = 512;
	p.tileResolution = 33;

	GridVertexInfo info = ReadGridVertexInfo(InstancingBuffer[instanceID], vertexID, ResourceDescriptorHeap[TexIdHeightmap], LinearSampler, p);

	GS_Input result;
	result.wp = info.position.xyz;
	result.uv = info.uv;
	return result;
}
#else
struct VS_Input
{
	float4 p : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
};

GS_Input VS_Main(VS_Input vin)
{
	GS_Input o;
	o.wp = mul(vin.p, WorldMatrix).xyz;
	o.normal  = mul(vin.normal, (float3x3)WorldMatrix);
	o.uv = vin.uv;

	return o;
}
#endif


[maxvertexcount(3)]
void GS_Main(triangle GS_Input input[3], inout TriangleStream<PS_Input> triStream)
{
	// 1. Triangle normal (world space)
	float3 e0 = input[1].wp - input[0].wp;
	float3 e1 = input[2].wp - input[0].wp;
	float3 n  = normalize(cross(e0, e1));
	float3 an = abs(n);

	// 2. Dominant axis: 0 = X, 1 = Y, 2 = Z
	uint axis = (an.x > an.y && an.x > an.z) ? 0 :
				(an.y > an.z) ? 1 : 2;

	// Voxel volume params
	float3 worldSize = VoxelInfo.Voxels[VoxelIdx].WorldSize;
	float3 worldOffset  = VoxelInfo.Voxels[VoxelIdx].Offset;

	// 3. Emit 3 vertices
	[unroll]
	for (int i = 0; i < 3; ++i)
	{
		PS_Input o;

		// World position
		float3 wp = input[i].wp;

		// 3a. World -> voxel coords
		// 3b. Normalize to [0,1]³
		float3 vNorm = (wp - worldOffset) / worldSize;

		// 3c. Map to clip space [-1,1]³ and swizzle by axis
		float3 clipPos;

		if (axis == 2) // Z-dominant: project onto XY, depth = Z
		{
			clipPos = vNorm;
		}
		else if (axis == 0) // X-dominant: project onto YZ, depth = X
		{
			clipPos = vNorm.yzx;
		}
		else // axis == 1, Y-dominant: project onto XZ, depth = Y
		{
			clipPos = vNorm.xzy;
		}

		o.pos = float4(clipPos.xy * 2.0f - 1.0f, clipPos.z, 1.0f);
		o.wp = float4(wp, 1.0f);
		o.uv = input[i].uv;
	#ifndef GRID
		o.normal = input[i].normal;
	#endif

		triStream.Append(o);
	}
}

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

Texture3D<float4> GetTexture3D(uint index)
{
	return ResourceDescriptorHeap[index];
}

float readShadowmap(Texture2D shadowmap, float2 shadowCoord)
{
	return shadowmap.SampleLevel(ShadowSampler, shadowCoord, 0).r;
}

float getShadow(float4 wp)
{
	Texture2D shadowmap = GetTexture(ShadowMapIdx);
	float4 sunLookPos = mul(wp, ShadowMatrix);
	sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
	sunLookPos.xy += 0.5;
	sunLookPos.z -= 0.0001;

	return readShadowmap(shadowmap, sunLookPos.xy) < sunLookPos.z ? 0.0 : 1.0;
}


float4 PS_Main(PS_Input pin) : SV_TARGET
{
	SamplerState sampler = SamplerDescriptorHeap[0];

#ifdef GRID
	float3 worldNormal = ReadGridNormal(ResourceDescriptorHeap[TexIdNormalmap], LinearSampler, pin.uv);
	float3 diffuse = float3(0.6, 0.6, 0.6);
	float3 green = float3(0.5, 0.55, 0.3);
	diffuse = lerp(diffuse, green, step(0.9,worldNormal.y));
#else
	float3x3 worldMatrix = (float3x3)WorldMatrix;
	float3 worldNormal = normalize(mul(pin.normal, worldMatrix));

	float3 diffuse = MaterialColor * GetTexture(TexIdDiffuse).Sample(sampler, pin.uv).rgb;
#endif

	float shadow = getShadow(pin.wp);
	if (shadow > 0)
		shadow = -dot(Sun.Direction,worldNormal);
	
	float3 voxelWorldPos = (pin.wp.xyz - VoxelInfo.Voxels[VoxelIdx].Offset);
	float3 posUV = voxelWorldPos * VoxelInfo.Voxels[VoxelIdx].Density;

	const float StepSize = 32.f;
	Texture3D SceneVoxelPrevious = GetTexture3D(VoxelInfo.Voxels[VoxelIdx].TexIdPrev);
	float3 prevUv = posUV - VoxelInfo.Voxels[VoxelIdx].MoveOffset * StepSize;
	float4 prev = SceneVoxelPrevious.Load(float4(prevUv, 0));

	RWTexture3D<float4> SceneVoxel = ResourceDescriptorHeap[VoxelInfo.Voxels[VoxelIdx].TexId];
	SceneVoxel[posUV] = prev;

	bool isInBounds = (posUV.x >= 0 && posUV.x < 128) &&
					  (posUV.y >= 0 && posUV.y < 128) &&
					  (posUV.z >= 0 && posUV.z < 128);

	if (isInBounds)
	{
		uint linearIndex = uint(posUV.z) * 128 * 128 + uint(posUV.y) * 128 + uint(posUV.x);

		InterlockedMax(SceneVoxelData[linearIndex].Diffuse, PackRGBA8(float4(diffuse, shadow)));
		InterlockedMax(SceneVoxelData[linearIndex].Normal, PackRGB10A2_SNORM(worldNormal.xyzz));
	}

	return float4(diffuse * shadow, 1);
}
