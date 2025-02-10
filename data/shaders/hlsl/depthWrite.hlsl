#ifdef BRANCH_WAVE
#include "TreeCommon.hlsl"
#endif

float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;

#ifdef BRANCH_WAVE
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