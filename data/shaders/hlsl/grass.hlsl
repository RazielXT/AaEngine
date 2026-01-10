#include "VoxelConeTracingCommon.hlsl"
#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 InvViewMatrix;
uint2 ViewportSize;
float Time;
float DeltaTime;
float3 CameraPosition;
uint TexIdDiffuse;

#ifdef ENTITY_ID
uint EntityId;
#endif

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

cbuffer SceneVoxelInfo : register(b2)
{
    SceneVoxelCbufferIndexed VoxelInfo;
};

struct GrassVertex { float3 pos[2]; float3 color; float3 normal; float scale; };

StructuredBuffer<GrassVertex> GeometryBuffer : register(t0);

struct PSInput
{
    float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 color : TEXCOORD1;
	float4 worldPosition : TEXCOORD2;
	float3 normal : TEXCOORD3;
	float4 currentPosition : TEXCOORD4;
	float4 previousPosition : TEXCOORD5;
	float3 terrainNormal : TEXCOORD6;
	float depth : TEXCOORD7;
};

static const float2 coords[4] = {
    float2(1.0f, 1.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),	
    float2(0.0f, 0.0f)
};

float randomGrassOffset(uint idx, float seed)
{
	return (((idx % seed) / (seed - 1)) * 2 - 1)  * 0.5f;
}

PSInput VSMain(uint vertexIdx : SV_VertexId, uint instanceId : SV_InstanceID)
{
    PSInput output;

    uint vertexID = vertexIdx % 4;
	float2 uv = coords[vertexID];

	GrassVertex v = GeometryBuffer[instanceId];
	
	const float grassHeight = 1;
	float top = 1 - uv.y;

#ifndef BILLBOARDS
	//float3 basePos = v.pos[uv.x];
	float3 posOffset = (v.pos[0] + v.pos[1]) * 0.5f;
	//posOffset.y = max(v.pos[0].y, v.pos[1].y);

	float3 pos = float3((uv.x * 2 - 1) * 0.5f, 0, top * grassHeight);	
	const float3 faceDir = float3(v.scale ,0, 0);
	float3 basisX = mul(faceDir, (float3x3)InvViewMatrix);
	const float3 worldUp = float3(0, 1, 0);
	float3 basisY = normalize(cross(basisX, worldUp));
	float3 basisZ = cross(basisY, basisX);
	float3x3 transformMat = float3x3(basisX, basisY, basisZ);
	pos = mul(pos, transformMat);
	
	float3 offsetDiff = v.pos[0] - v.pos[1];
	float yDiff = offsetDiff.y;
	float dotDiff = dot(normalize(offsetDiff), normalize(pos));
	
	pos += posOffset;
	pos.y += (dotDiff * yDiff);

	pos.x += randomGrassOffset(instanceId + vertexIdx, 7);
	pos.z += randomGrassOffset(instanceId + vertexIdx, 13);

#else
	float3 pos = v.pos[uv.x];
	pos.y += 0.5 + top * grassHeight;
#endif

	const float windStrength = 0.2;
	float3 windDirection = float3(1,0,1) * windStrength;
	float frequency = 1;
	float scale = 1;
	float3 windEffect = top * windDirection * sin(Time * frequency + (pos.x + pos.z + pos.y) * scale);

	float3 previousPos = pos;
	pos.xyz += windEffect;
	
	float3 previousWindEffect = top * windDirection * sin((Time - DeltaTime) * frequency + (previousPos.x + previousPos.z + previousPos.y) * scale);
	previousPos.xyz += previousWindEffect;

    // Output position and UV
	output.worldPosition = float4(pos,1);
    output.position = mul(output.worldPosition, ViewProjectionMatrix);
    output.uv = uv;
	output.color = v.color;
	
	output.currentPosition = output.position;
	output.previousPosition = mul(float4(previousPos,1), ViewProjectionMatrix);

    // Calculate the normal
    float3 up = float3(0, grassHeight, 0) + windEffect;
	float3 right = v.pos[0] - v.pos[1];
    float3 normal = normalize(cross(up, right));
    output.normal = normal;
	output.terrainNormal = v.normal;

	output.depth = output.position.z / output.position.w;

    return output;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

Texture3D<float4> GetTexture3D(uint index)
{
    return ResourceDescriptorHeap[index];
}

float getDistanceBlend(float dist, float sceneSize)
{
	sceneSize *= 0.5f;
	float maxDistance = sceneSize * 0.75f;

	const float fadeStart = maxDistance * 0.8f;
	const float fadeRange = maxDistance * 0.2f;

	return saturate((dist - fadeStart) / fadeRange); 
}

SamplerState ShadowSampler : register(s0);
SamplerState VoxelSampler : register(s1);

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
	//float depth : SV_Depth;
};

static const float AlphaThreshold = 0.8f;
static const float FadeThreshold = 256.f * 0.75f;

float TransitionDepth(float uvX)
{
    return 0.5 + 0.5 * sin(uvX * 20.0 * 3.14159);
}

PSOutput PSMain(PSInput input)
{
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);

	if (albedo.a < AlphaThreshold) discard;

	float camDistance = length(CameraPosition - input.worldPosition.xyz);

	float distanceFade = camDistance / FadeThreshold;
	if (albedo.a < distanceFade) discard;

	float shadowing = getPssmShadow(input.worldPosition, camDistance, 1, ShadowSampler, Sun);
	float lighting = dot(-Sun.Direction,input.terrainNormal);

	float3 shading = shadowing * saturate(lighting);

	albedo.rgb *= input.color;

	float3 voxelAmbient = 0;
	{
		float voxelWeight = 1.0f;
		float fullWeight = 0.01f;
		float3 worldNormal = input.terrainNormal;

		for (int idx = 0; idx < 4; idx++)
		{
			float3 voxelUV = (input.worldPosition.xyz - VoxelInfo.Voxels[idx].Offset) / VoxelInfo.Voxels[idx].WorldSize;
			Texture3D voxelmap = GetTexture3D(VoxelInfo.Voxels[idx].TexId);
			
			float mipOffset = idx == 3 ? 0 : -1;
			float4 fullTrace = ConeTraceImpl(voxelUV, worldNormal, VoxelInfo.MiddleConeRatio.x, VoxelInfo.MiddleConeRatio.y, voxelmap, VoxelSampler);

			if (idx < 3)
				fullTrace *= 1 - getDistanceBlend(camDistance, VoxelInfo.Voxels[idx].WorldSize);

			voxelAmbient += fullTrace.rgb * voxelWeight;
			{
				voxelWeight = saturate(voxelWeight - fullTrace.w);
				fullWeight += fullTrace.w;
			}
		}

		voxelWeight = pow(1 - voxelWeight, 2);

		voxelAmbient /= max(0.2f, voxelWeight);
		fullWeight /= 4.0f;
		fullWeight = pow(1 - fullWeight, 1);
		
		float3 skyAmbient = float3(0.2, 0.2, 0.3) * (0.15 + 0.5 * saturate(worldNormal.y));
		float3 skyLighting = fullWeight * skyAmbient;
		voxelAmbient = saturate(voxelAmbient + skyLighting);
	}
	
	float positionAmbient = (1 - input.uv.y) * 0.5 + 0.5;

	albedo.rgb *= (shading + voxelAmbient) * positionAmbient;

	PSOutput output;
    output.color = albedo;
	output.normals = float4(input.normal, 1);
	output.camDistance = float4(camDistance, 0, 0, 0);

	output.motionVectors = float4((input.previousPosition.xy / input.previousPosition.w - input.currentPosition.xy / input.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;
	
	/*
    float4 adjposition = mul(input.worldPosition + float4(TransitionDepth(input.uv.x), 0,0,0), ViewProjectionMatrix);
	output.depth = adjposition.z / adjposition.w;
*/
	return output;
}

struct PSOutputDepth
{
	//float depth : SV_Depth;
};

void PSMainDepth(PSInput input)
{
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);

	if (albedo.a < AlphaThreshold) discard;
	
	float camDistance = length(CameraPosition - input.worldPosition.xyz);
	float distanceFade = camDistance / FadeThreshold;

	if (albedo.a < distanceFade) discard;
	
	/*PSOutputDepth output;
    float4 adjposition = mul(input.worldPosition + float4(TransitionDepth(input.uv.x), 0,0,0), ViewProjectionMatrix);
	output.depth = adjposition.z / adjposition.w;
	
	return output;*/
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
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);

	if (albedo.a < AlphaThreshold) discard;
	
	float camDistance = length(CameraPosition - input.worldPosition.xyz);
	float distanceFade = camDistance / FadeThreshold;

	if (albedo.a < distanceFade) discard;

	PSOutputId output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = float4(input.worldPosition.xyz, 0);
	output.normal = float4(input.normal.xyz, 0);
	return output;
}

#endif
