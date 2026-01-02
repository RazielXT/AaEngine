#ifdef BRANCH_WAVE
#include "TreeCommon.hlsl"
#endif

float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;

#ifdef BRANCH_WAVE
float Time;
#endif

#ifdef VERTEX_WAVE
float Time;
#endif

#ifdef ALPHA_TEST
uint TexIdDiffuse;
#endif

#ifdef INSTANCED
StructuredBuffer<float4x4> InstancingBuffer : register(t0);
#endif

struct VS_INPUT
{
    float4 position : POSITION;
#ifdef INSTANCED
	uint instanceID : SV_InstanceID;
#endif

#ifdef ALPHA_TEST
	float2 uv : TEXCOORD;
#endif
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
#ifdef ALPHA_TEST
	float2 uv : TEXCOORD;
#endif
};

#ifdef VERTEX_WAVE
float3 WindWave(float3 basePos, float swayFactor, float seed)
{
	const float3 WindDirection = float3(1,0.5,0);
	const float  WindStrength = 2.5f;
	const float  WindSpeed = 3;

	float leafRandom = (basePos.z + basePos.x)/5;
	float phase = WindSpeed * seed + leafRandom + swayFactor * 3;
	float windOffset = ((sin(phase*0.4 + 0.5) + sin(phase)) * 0.25 + 0.75) * WindStrength;

	float3 windDisplacement = WindDirection * windOffset * swayFactor;
	return basePos + windDisplacement;
}

void ComputeBaseBend(float4 basePosOS, inout float4 vertexPosOS, float w)
{
	vertexPosOS.y -= dot(basePosOS.xz, basePosOS.xz) * 0.15 * w;
}
#endif

VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;

#ifdef INSTANCED
	float4 worldPosition = mul(Input.position, InstancingBuffer[Input.instanceID]);
#else
	float4 worldPosition = mul(Input.position, WorldMatrix);
#endif

#ifdef BRANCH_WAVE
	worldPosition.xyz += getBranchWaveOffset(Time, Input.position.xyz, Input.uv);
#endif

#ifdef VERTEX_WAVE
	worldPosition.xyz = WindWave(worldPosition.xyz, 1-Input.uv.y, Time);
#endif

	Output.position = mul(worldPosition, ViewProjectionMatrix);

#ifdef ALPHA_TEST
	Output.uv = Input.uv;
#endif

    return Output;
}

#ifdef ALPHA_TEST

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

void PSMain(VS_OUTPUT input)
{
	SamplerState sampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(sampler, input.uv);

	if (albedo.a <0.37) discard;
}
#endif