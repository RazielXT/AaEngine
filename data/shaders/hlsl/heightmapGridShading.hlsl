#define GRID_INDEX_DEBUG
#if defined(GRID_LOD_DEBUG) || defined(GRID_INDEX_DEBUG)
#define GRID_DEBUG_COLOR
#endif

#include "grid/heightmapGridReconstruction.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/MotionVectors.hlsl"
#include "hlsl/common/ShaderOutputs.hlsl"
#include "hlsl/common/Triplanar.hlsl"

float4x4 ViewProjectionMatrix;
float3 MaterialColor;
float3 CameraPosition;
float3 WorldPosition;
uint TexIdHeightmap;
uint TexIdNormalmap;
uint TexIdDiffuse;
uint TexIdGrass;
uint TexIdNormal;
uint TexIdGrassNormal;
uint TexIdSpread;
float2 GridHeightWidth;

#ifdef ENTITY_ID
uint EntityId;
#endif

#ifdef GRID_INDEX_DEBUG
uint GridIndex;
#endif

StructuredBuffer<GridTileData> InstancingBuffer : register(t0);

SamplerState LinearWrapSampler : register(s0);

struct PSInput
{
	float4 position : SV_POSITION;
	float4 worldPosition : TEXCOORD0;
	float2 uv : TEXCOORD1;
#ifdef GRID_DEBUG_COLOR
	float3 debugColor : TEXCOORD10;
#endif
};

PSInput VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	GridRuntimeParams p;
	p.cameraPos = CameraPosition;
	p.worldPos = WorldPosition;
	p.heightScale = GridHeightWidth.x;
	p.gridSize = GridHeightWidth.y;
	p.tilesWidth = 512;
	p.tileResolution = 33;

	GridVertexInfo info = ReadGridVertexInfo(InstancingBuffer[instanceID], vertexID, ResourceDescriptorHeap[TexIdHeightmap], LinearWrapSampler, p);

	float heightTexture = GetTexture2D(TexIdGrass).SampleLevel(LinearWrapSampler, info.uv * 50 * 10, 0).w;
	info.position.y -= heightTexture * 0.0001;

	PSInput result;
	result.worldPosition = info.position;
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.uv = info.uv;

#ifdef GRID_DEBUG_COLOR
	static const float3 LODColors[10] = {
		float3(1.00, 0.00, 0.00), // Red
		float3(0.00, 0.60, 0.10), // Bright Green (very bright)
		float3(0.00, 0.20, 1.00), // Sky Blue (bright)
		float3(1.00, 0.80, 0.00), // Gold (bright)
		float3(0.20, 0.00, 1.00), // Deep Blue (dark)
		float3(1.00, 0.20, 0.60), // Hot Pink (bright)
		float3(0.60, 0.30, 0.00), // Brown/Amber (dark)
		float3(0.00, 1.00, 0.40), // Green-Teal (bright)
		float3(0.80, 0.00, 0.80), // Purple (mid)
		float3(1.00, 1.00, 1.00)  // White (max contrast)
	};
	#ifdef GRID_LOD_DEBUG
		result.debugColor = (1 - 0.4 * info.morphMask) * LODColors[9-InstancingBuffer[instanceID].lod];
	#elif defined(GRID_INDEX_DEBUG)
		result.debugColor = LODColors[GridIndex % 9];
	#endif
#endif
	return result;
}


float3 CreateDistanceWeigths4(float camDistance, float distances[4], float scales[4])
{
	float lerpWeight = 1;
	float closeDetailUv = 1;
	float farDetailUv = scales[0];

	for (int i = 0; i < 3; i++)
	{
		lerpWeight = saturate((camDistance - distances[i]) / (distances[i + 1] - distances[i]));
		closeDetailUv = farDetailUv;
		farDetailUv = scales[i + 1];
		
		if (lerpWeight < 1.f)
			break;
	}
	
	return float3(closeDetailUv, farDetailUv, lerpWeight);
}

float4 ReadDistanceTexture(uint texId, SamplerState sampler, float2 uv, float3 weights)
{
	float4 normalTexClose = GetTexture2D(texId).Sample(sampler, uv * weights.x);
	float4 normalTex = GetTexture2D(texId).Sample(sampler, uv * weights.y);

	return float4(lerp(normalTexClose.rgb, normalTex.rgb, weights.z), normalTexClose.a);
}

float3 UnpackNormal(float3 n)
{
    return float3(n.xy * 2 - 1, n.z);
}

float3 BlendUDN(float3 n1, float3 n2)
{
    return normalize(float3(n1.xy + n2.xy, n1.z * n2.z));
}

GBufferOutput PSMain(PSInput input)
{
	SamplerState sampler = GetDynamicMaterialSamplerLinear();
	float3 cameraView = CameraPosition - input.worldPosition.xyz;
	float camDistance = length(cameraView);

	GridTBN tbn = ReadGridTBN(ResourceDescriptorHeap[TexIdNormalmap], sampler, input.uv);

	float2 texCoords = input.uv * 50;

	//float diffuseStepDistance[4] = { 0, 50, 2000, 6000 };
	//float diffuseStepScale[4] = { 4, 1.f/2, 1.f/5, 1.f/20 };
	//float3 diffuseDistanceWeights = CreateDistanceWeigths4(camDistance, diffuseStepDistance, diffuseStepScale);
	//float4 rockTexture = ReadDistanceTexture(TexIdDiffuse, sampler, texCoords, diffuseDistanceWeights);

	//float4 rockFarTexture = GetTexture(TexIdDiffuse).Sample(sampler, texCoords / 10.f);
	//float rockFarWeight = saturate((camDistance - 500) / 1000);
	float triplanarScale = 0.03;
	float triplanarScaleFar = triplanarScale * 0.1;
	float rockDistanceWeight = saturate(camDistance / 300);

	float4 rockTextureNear = SampleTriplanar(TexIdDiffuse, sampler, input.worldPosition.xyz, tbn.N, triplanarScale);
	float4 rockTextureFar = SampleTriplanar(TexIdDiffuse, sampler, input.worldPosition.xyz, tbn.N, triplanarScaleFar);
	float4 rockTexture = lerp(rockTextureNear, rockTextureFar, rockDistanceWeight);

	float3 rock = rockTexture.rgb * float3(0.95, 1, 0.9) * 0.7;

	float4 grassTexture = GetTexture2D(TexIdGrass).Sample(sampler, texCoords * 10);

	float3 mud = (rockTexture.rgb + grassTexture.ggg) * 0.5 * float3(0.4, 0.2, 0.1) * 0.8; // Color for brown
	float3 grass = grassTexture.rgb * float3(0.6, 0.9, 0.3) * 0.5;

	float heightWeight = saturate((input.worldPosition.y - WorldPosition.y) / GridHeightWidth.x);
	heightWeight = 1 - smoothstep(0.1,0.6,heightWeight);
	float normalWeight = smoothstep(0.3,1,tbn.N.y*tbn.N.y);//saturate(normal.y - 0.5) * 2;

	float weightFunc = normalWeight * heightWeight + (1-rockTextureNear.w) * (0.2 + normalWeight * 0.3);

	float layerBlendDistance = saturate(rockDistanceWeight * rockDistanceWeight);
	float rockWeight = smoothstep(0.245 - layerBlendDistance * 0.05, 0.25 + layerBlendDistance * 0.06, weightFunc);//smoothstep(0.2, 1, max(detailSpread, normalWeight * normalWeight * normalWeight));
	float grassWeight = smoothstep(0.145 - layerBlendDistance * 0.05, 0.15, weightFunc * min(rockTextureNear.w, rockTextureFar.w) * 0.8);

	float3 albedoMid = lerp(mud, grass, grassWeight);
	float3 albedo = lerp(rock, albedoMid, rockWeight);

	float grassDistanceWeight = saturate((camDistance - 50) / 800);
	float3 grassNormalTexture = lerp(GetTexture2D(TexIdGrassNormal).Sample(sampler, texCoords * 10), GetTexture2D(TexIdGrassNormal).Sample(sampler, texCoords * 2), grassDistanceWeight).rgb * 2 - 1;
	grassNormalTexture.y = -grassNormalTexture.y;
	float3 normal = normalize(
		tbn.T * grassNormalTexture.x +
		tbn.B * grassNormalTexture.y +
		tbn.N * grassNormalTexture.z
	);

	float3 rockNormalTextureNear = SampleTriplanarNormal(TexIdNormal, sampler, input.worldPosition.xyz, tbn.N, triplanarScale);
	float3 rockNormalTextureFar = SampleTriplanarNormal(TexIdNormal, sampler, input.worldPosition.xyz, tbn.N, triplanarScaleFar);
	//rockNormalTextureNear = (rockNormalTextureNear + rockNormalTextureFar) / 2;
	float3 rockNormalTexture = lerp(rockNormalTextureNear, rockNormalTextureFar, rockDistanceWeight);
	normal = lerp(rockNormalTexture, normal, grassWeight);

	//normal = BlendUDN(rockNormalTexture, normal * grassWeight);
	//N = BlendUDN(N, UnpackNormal(detailN) * detailWeight);
	normal = normalize(normal);

#ifdef GRID_DEBUG_COLOR
	albedo = lerp(albedo, input.debugColor, 0.7);
#endif

	GBufferOutput output;
	output.albedo = float4(albedo,0);
	output.normals = float4(normal, 1);
	output.motionVectors = ZeroMotionVector();
	return output;
}

#ifdef ENTITY_ID

EntityIdOutput PSMainEntityId(PSInput input)
{
	EntityIdOutput output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = input.worldPosition;
	output.normal = float4(ReadGridNormal(ResourceDescriptorHeap[TexIdNormalmap], LinearWrapSampler, input.uv), 0);
	return output;
}

#endif
