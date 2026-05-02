#include "hlsl/sky/SunParams.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/MotionVectors.hlsl"
#include "hlsl/common/ShaderOutputs.hlsl"
#include "hlsl/common/Triplanar.hlsl"
#include "hlsl/common/DebugColors.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 PreviousWorldMatrix;
float4x4 WorldMatrix;
float3 MaterialColor;
uint TexIdDiffuse;
float3 CameraPosition;
uint2 ViewportSize;
#ifdef VERTEX_WAVE
float Time;
float DeltaTime;
#endif

#ifdef ENTITY_ID
uint EntityId;
#endif

#ifdef INSTANCED
StructuredBuffer<float4x4> InstancingBuffer : register(t0);
	#ifdef CHUNK_DEBUG_COLOR
	uint ChunkId;
	#endif
#endif

struct VSInput
{
	float4 position : POSITION;
	float3 normal : NORMAL;
#ifndef NO_TEXTURE
	float2 uv : TEXCOORD;
#endif
#ifdef USE_VC
	float4 color : COLOR;
#endif
#ifdef INSTANCED
	uint instanceID : SV_InstanceID;
#endif
};

struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
#ifdef USE_VC
	float4 color : COLOR;
#endif
#ifndef NO_TEXTURE
	float2 uv : TEXCOORD0;
#endif
	float4 worldPosition : TEXCOORD1;
	float4 previousPosition : TEXCOORD2;
	float4 currentPosition : TEXCOORD3;
};

float3 WindWave(float3 basePos, float swayFactor, float time)
{
	const float3 WindDir = normalize(float3(1, 0.5, 0));
	const float WindStrength = 0.5f;
	const float WindSpeed = 2;

	float timeWeight = WindSpeed * time;

	float w1 = sin((basePos.x * 2 + basePos.z) + timeWeight);
	float w2 = sin((basePos.z + basePos.y) * 4.0 + timeWeight);
	float wind = w1 - w2;

	// Normalize to 0..1
	wind = wind * 0.5 + 0.5;

	float windOffset = wind * WindStrength * swayFactor;

	return basePos + WindDir * windOffset;
}


void ComputeBaseBend(float4 basePosOS, inout float4 vertexPosOS, float w)
{
	vertexPosOS.y -= dot(basePosOS.xz, basePosOS.xz) * 0.15 * w;
}

PSInput VSMain(VSInput input)
{
	PSInput result;

#ifdef INSTANCED
	result.worldPosition = mul(input.position, InstancingBuffer[input.instanceID]);
	#ifdef VERTEX_WAVE
		const float WaveFade = 200.f;
		float waveWeight = saturate(1 - length(CameraPosition - result.worldPosition.xyz)/WaveFade) * (1-input.uv.y);
		ComputeBaseBend(input.position, result.worldPosition, waveWeight);
		float4 worldPositionNoWave = result.worldPosition;
		result.worldPosition.xyz = WindWave(result.worldPosition.xyz, waveWeight, Time);
	#endif
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = normalize(mul(input.normal, (float3x3)InstancingBuffer[input.instanceID]));
	
	#ifdef VERTEX_WAVE
		float4 previousWorldPosition = float4(WindWave(worldPositionNoWave.xyz, waveWeight, Time - DeltaTime), 1);
		result.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	#else
		result.previousPosition = result.position; // no support
	#endif
	result.currentPosition = result.position;
#else
	result.worldPosition = mul(input.position, WorldMatrix);
	#ifdef VERTEX_WAVE
		result.worldPosition.xyz = WindWave(result.worldPosition.xyz, 1-input.uv.y, Time);
	#endif
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = normalize(mul(input.normal, (float3x3)WorldMatrix));

	float4 previousWorldPosition = mul(input.position, PreviousWorldMatrix);
	#ifdef VERTEX_WAVE
		previousWorldPosition.xyz = WindWave(previousWorldPosition.xyz, 1-input.uv.y, Time - DeltaTime);
	#endif
	result.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	result.currentPosition = result.position;
#endif

#ifdef USE_VC
	result.color = input.color;
#endif

#ifndef NO_TEXTURE
	result.uv = input.uv;
	#ifndef VERTEX_WAVE
		//result.uv *= 10;
	#endif
#endif

	return result;
}

GBufferOutput PSMain(PSInput input)
{
	SamplerState sampler = GetDynamicMaterialSamplerLinear();

#ifndef NO_TEXTURE

	#ifdef TRIPLANAR
	float4 albedoTex = SampleTriplanar(TexIdDiffuse, sampler, input.worldPosition.xyz, input.normal.xyz, 0.05);
	#else
	float4 albedoTex = GetTexture2D(TexIdDiffuse).Sample(sampler, input.uv);
	#endif

	#if defined(ALPHA_TEST) && !defined(FLAT_COLOR)
		if (albedoTex.a < 0.4)
			discard;
	#endif

	float3 albedo = albedoTex.rgb * MaterialColor;
#else
	float3 albedo = MaterialColor;
#endif

#ifdef CHUNK_DEBUG_COLOR
	albedo = lerp(albedo, GetDebugColor(ChunkId), 0.7);
#endif

#ifdef INSTANCED
	albedo = lerp(albedo, float3(0.7,0.6,0.1), 0.1);
#endif

	GBufferOutput output;
	output.albedo = float4(albedo,0);
	output.normals = float4(input.normal, 1);
	output.motionVectors = EncodeMotionVector(input.previousPosition, input.currentPosition, ViewportSize);
	return output;
}

#ifdef ENTITY_ID
EntityIdOutput PSMainEntityId(PSInput input)
{
#ifdef ALPHA_TEST
	SamplerState sampler = GetDynamicMaterialSamplerLinear();
	float4 albedoTex = GetTexture2D(TexIdDiffuse).Sample(sampler, input.uv);
	if (albedoTex.a < 0.4)
		discard;
#endif

	EntityIdOutput output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = float4(input.worldPosition.xyz, 0);
	output.normal = float4(input.normal, 0);
	return output;
}
#endif

#ifdef DEPTH
void PSMainDepth(PSInput input)
{
#ifdef ALPHA_TEST
	SamplerState sampler = GetDynamicMaterialSamplerLinear();
	float4 albedoTex = GetTexture2D(TexIdDiffuse).Sample(sampler, input.uv);
	if (albedoTex.a < 0.4)
		discard;
#endif
}
#endif
