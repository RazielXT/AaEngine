#include "AnisoSeparateVoxelConeTracingCommon.hlsl"
#include "hlsl/sky/SkyParams.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"

#ifdef GRID
#define GRID_PADDING
#include "hlsl/grid/heightmapGridReconstruction.hlsl"
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
float Emission;

cbuffer SceneVoxelInfo : register(b1)
{
	AnisoSeparateSceneVoxelCbufferIndexed VoxelInfo;
};

cbuffer SkyParamsBuffer : register(b2)
{
	SkyParams Sky;
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
	float4 pos : SV_POSITION;
	float4 wp : TEXCOORD1;
	float2 uv : TEXCOORD0;
#ifndef GRID
	float3 normal : NORMAL;
#endif
};

#ifdef GRID
StructuredBuffer<GridTileData> InstancingBuffer : register(t0);

GS_Input VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
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

GS_Input VSMain(VS_Input vin)
{
	GS_Input o;
	o.wp = mul(vin.p, WorldMatrix).xyz;
	o.normal  = mul(vin.normal, (float3x3)WorldMatrix);
	o.uv = vin.uv;

	return o;
}
#endif

[maxvertexcount(3)]
void GSMain(triangle GS_Input input[3], inout TriangleStream<PS_Input> triStream)
{
	float3 e0 = input[1].wp - input[0].wp;
	float3 e1 = input[2].wp - input[0].wp;
	float3 n  = normalize(cross(e0, e1));
	float3 an = abs(n);

	uint axis = (an.x > an.y && an.x > an.z) ? 0 :
				(an.y > an.z) ? 1 : 2;

	float3 worldSize = VoxelInfo.Voxels[VoxelIdx].WorldSize;
	float3 worldOffset  = VoxelInfo.Voxels[VoxelIdx].Offset;

	[unroll]
	for (int i = 0; i < 3; ++i)
	{
		PS_Input o;

		float3 wp = input[i].wp;
		float3 vNorm = (wp - worldOffset) / worldSize;
		float3 clipPos;

		if (axis == 2)
		{
			clipPos = vNorm;
		}
		else if (axis == 0)
		{
			clipPos = vNorm.yzx;
		}
		else
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

float readShadowmap(Texture2D shadowmap, float2 shadowCoord)
{
	return shadowmap.SampleLevel(ShadowSampler, shadowCoord, 0).r;
}

float getShadow(float4 wp)
{
	Texture2D shadowmap = GetTexture2D(ShadowMapIdx);
	float4 sunLookPos = mul(wp, ShadowMatrix);
	sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
	sunLookPos.xy += 0.5;
	sunLookPos.z -= 0.0001;

	return readShadowmap(shadowmap, sunLookPos.xy) < sunLookPos.z ? 0.0 : 1.0;
}

float4 PSMain(PS_Input pin) : SV_TARGET
{
	SamplerState sampler = GetDynamicMaterialSamplerLinear();

#ifdef GRID
	float3 worldNormal = ReadGridNormal(ResourceDescriptorHeap[TexIdNormalmap], LinearSampler, pin.uv);
	float3 diffuse = float3(0.8, 0.8, 0.8);
	float3 green = float3(0.4, 0.6, 0.3);
	diffuse = lerp(diffuse, green, step(0.9,worldNormal.y));
#else
	float3x3 worldMatrix = (float3x3)WorldMatrix;
	float3 worldNormal = normalize(mul(pin.normal, worldMatrix));

	float3 diffuse = MaterialColor * GetTexture2D(TexIdDiffuse).Sample(sampler, pin.uv).rgb;
#endif

	float shadow = getShadow(pin.wp);
	if (shadow > 0)
		shadow = -dot(Sky.SunDirection,worldNormal);

	float3 voxelWorldPos = (pin.wp.xyz - VoxelInfo.Voxels[VoxelIdx].Offset);
	float3 posUV = voxelWorldPos * VoxelInfo.Voxels[VoxelIdx].Density;

	const float StepSize = 32.f;
	float3 prevUv = posUV - VoxelInfo.Voxels[VoxelIdx].MoveOffset * StepSize;

	[unroll]
	for (uint f = 0; f < 6; f++)
	{
		Texture3D<float4> prevFace = GetTexture3D(GetPrevFaceTexId(VoxelInfo.Voxels[VoxelIdx], f));
		float3 prevColor = prevFace.Load(float4(prevUv, 0)).rgb;

		RWTexture3D<float3> currentFace = ResourceDescriptorHeap[GetFaceTexId(VoxelInfo.Voxels[VoxelIdx], f)];
		currentFace[posUV] = prevColor;
	}

	Texture3D<float> prevOccupancy = GetTexture3D1f(GetPrevOccupancyTexId(VoxelInfo.Voxels[VoxelIdx]));
	RWTexture3D<float> currentOccupancy = ResourceDescriptorHeap[GetOccupancyTexId(VoxelInfo.Voxels[VoxelIdx])];
	currentOccupancy[posUV] = prevOccupancy.Load(float4(prevUv, 0)).r;

	bool isInBounds = (posUV.x >= 0 && posUV.x < 128) &&
					  (posUV.y >= 0 && posUV.y < 128) &&
					  (posUV.z >= 0 && posUV.z < 128);

	if (isInBounds)
	{
		uint linearIndex = uint(posUV.z) * 128 * 128 + uint(posUV.y) * 128 + uint(posUV.x);

		float visibility = max(shadow, Emission);
		InterlockedMax(SceneVoxelData[linearIndex].Diffuse, PackRGBA8(float4(visibility, diffuse)));
		InterlockedMax(SceneVoxelData[linearIndex].Normal, PackR11G10B11_SNORM(worldNormal.xyz));
	}

	return float4(diffuse * shadow, 1);
}