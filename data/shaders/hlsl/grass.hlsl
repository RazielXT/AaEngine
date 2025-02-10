#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
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

struct GrassVertex { float3 pos; float3 color; };

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
};

static const float2 coords[4] = {
    float2(1.0f, 1.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),	
    float2(0.0f, 0.0f)
};

static const uint indices[6] = {
    0, 1, 2, // First triangle
    2, 1, 3  // Second triangle
};

PSInput VSMain(uint vertexIdx : SV_VertexId)
{
    PSInput output;

	uint quadID = vertexIdx / 6;
    uint vertexID = vertexIdx % 6;
	float2 uv = coords[indices[vertexID]];

	GrassVertex v = GeometryBuffer[quadID * 2 + uv.x];
	float3 pos = v.pos;
	
	float top = 1 - uv.y;
	float grassHeight = 4;
	pos.y += top * grassHeight;

	float3 windDirection = float3(1,0,1);
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
	float3 right = GeometryBuffer[quadID * 2].pos - GeometryBuffer[quadID * 2 + 1].pos;
    float3 normal = normalize(cross(up, right));
    output.normal = normal;

    return output;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState ShadowSampler : register(s0);

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

PSOutput PSMain(PSInput input)
{
	SamplerState g_sampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, input.uv);

	float distanceFade = saturate(2000 * input.worldPosition.z / input.position.w);

	//albedo.a *= 2;

	if (albedo.a <0.5) discard;

	albedo.rgb *= input.color * 1.5;

	float camDistance = length(CameraPosition - input.worldPosition.xyz);

	float shadowing = getPssmShadowSimple(input.worldPosition, camDistance, 1, ShadowSampler, Sun);
	float shading = saturate(1.2 - input.uv.y + shadowing);
	shading *= lerp(0.5, 1, shadowing);
	
	PSOutput output;
    output.color = albedo * shading;
	output.normals = float4(input.normal, 1);
	output.camDistance = float4(camDistance, 0, 0, 0);

	output.motionVectors = float4((input.previousPosition.xy / input.previousPosition.w - input.currentPosition.xy / input.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;

	return output;
}

void PSMainDepth(PSInput input)
{
	SamplerState g_sampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, input.uv);

	float distanceFade = saturate(2000 * input.worldPosition.z / input.position.w);

	if (albedo.a <0.5) discard;
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
