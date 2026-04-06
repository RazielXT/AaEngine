#include "grid/heightmapGridReconstruction.hlsl"

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

//#define DEBUG

StructuredBuffer<GridTileData> InstancingBuffer : register(t0);

SamplerState LinearWrapSampler : register(s0);

struct PSInput
{
	float4 position : SV_POSITION;
	float4 worldPosition : TEXCOORD0;
	float2 uv : TEXCOORD1;
#ifdef DEBUG
	float debugColor : TEXCOORD10;
#endif
};

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

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

	float heightTexture = GetTexture(TexIdGrass).SampleLevel(LinearWrapSampler, info.uv * 50 * 10, 0).w;
	info.position.y -= heightTexture;

	PSInput result;
	result.worldPosition = info.position;
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.uv = info.uv;

#ifdef DEBUG
	result.debugColor = heightTexture;
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
	float4 normalTexClose = GetTexture(texId).Sample(sampler, uv * weights.x);
	float4 normalTex = GetTexture(texId).Sample(sampler, uv * weights.y);

	return float4(lerp(normalTexClose.rgb, normalTex.rgb, weights.z), normalTexClose.a);
}

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normals : SV_Target1;
	float4 motionVectors : SV_Target2;
};

float4 SampleTriplanar(uint texId, SamplerState sampler, float3 worldPos, float3 worldNormal, float scale)
{
	float3 blend_weights = abs(worldNormal.xyz);
	// Tighten up the blending zone:
	blend_weights = (blend_weights - 0.2) * 7;
	blend_weights = max(blend_weights, 0);
	blend_weights /= (blend_weights.x + blend_weights.y + blend_weights.z).xxx;

	float2 uvX = worldPos.yz * scale;
	float2 uvY = worldPos.zx * scale;
	float2 uvZ = worldPos.xy * scale;

	float4 col1 = GetTexture(texId).Sample(sampler, uvX);
	float4 col2 = GetTexture(texId).Sample(sampler, uvY);
	float4 col3 = GetTexture(texId).Sample(sampler, uvZ);

	return col1 * blend_weights.x + col2 * blend_weights.y + col3 * blend_weights.z;
}

float3 SampleTriplanarNormal(uint texId, SamplerState sampler, float3 worldPos, float3 worldNormal, float scale)
{
	float3 blend_weights = abs(worldNormal.xyz);
	// Tighten up the blending zone:
	blend_weights = (blend_weights - 0.2) * 7;
	blend_weights = max(blend_weights, 0);
	blend_weights /= (blend_weights.x + blend_weights.y + blend_weights.z).xxx;

	float2 uvX = worldPos.zy * scale;
	float2 uvY = worldPos.xz * scale;
	float2 uvZ = worldPos.xy * scale;

	// Tangent space normal maps
	float3 tnormalX = GetTexture(texId).Sample(sampler, uvX).rgb * 2 - 1;
	float3 tnormalY = GetTexture(texId).Sample(sampler, uvY).rgb * 2 - 1;
	float3 tnormalZ = GetTexture(texId).Sample(sampler, uvZ).rgb * 2 - 1;

	// Swizzle world normals into tangent space and apply UDN blend.
	// These should get normalized, but it's very a minor visual
	// difference to skip it until after the blend.
	tnormalX = float3(tnormalX.xy + worldNormal.zy, worldNormal.x);
	tnormalY = float3(tnormalY.xy + worldNormal.xz, worldNormal.y);
	tnormalZ = float3(tnormalZ.xy + worldNormal.xy, worldNormal.z);

	// Swizzle tangent normals to match world orientation and triblend
	return normalize(
		tnormalX.zyx * blend_weights.x +
		tnormalY.xzy * blend_weights.y +
		tnormalZ.xyz * blend_weights.z
	);
}

float3 UnpackNormal(float3 n)
{
    return float3(n.xy * 2 - 1, n.z);
}

float3 BlendUDN(float3 n1, float3 n2)
{
    return normalize(float3(n1.xy + n2.xy, n1.z * n2.z));
}

PSOutput PSMain(PSInput input)
{
	SamplerState sampler = SamplerDescriptorHeap[0];
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

	float4 grassTexture = GetTexture(TexIdGrass).Sample(sampler, texCoords * 10);

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
	float3 grassNormalTexture = lerp(GetTexture(TexIdGrassNormal).Sample(sampler, texCoords * 10), GetTexture(TexIdGrassNormal).Sample(sampler, texCoords * 2), grassDistanceWeight).rgb * 2 - 1;
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

#ifdef DEBUG
	albedo = input.debugColor; 	//float3(input.uv.xy, 0);
#endif

	PSOutput output;
	output.albedo = float4(albedo,0);
	output.normals = float4(normal, 1);
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
	output.normal = float4(ReadGridNormal(ResourceDescriptorHeap[TexIdNormalmap], LinearWrapSampler, input.uv), 0);
	return output;
}

#endif
