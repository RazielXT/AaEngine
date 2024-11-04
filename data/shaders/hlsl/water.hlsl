float4x4 WorldViewProjectionMatrix;
float4x4 WorldMatrix;
uint TexIdDiffuse;
uint TexIdHeight;
float Time;

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState g_sampler : register(s0);

float GetWavesSize(float2 uv)
{
	uv = uv * 0.5;
	uv.y += Time / 20;
	float4 albedo = GetTexture(TexIdHeight).SampleLevel(g_sampler, uv, 4);

	return albedo.r * 5;
}

struct VSInput
{
    float4 position : POSITION;
	float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

	float4 objPosition = input.position;
	objPosition.y += GetWavesSize(input.uv);

    result.position = mul(objPosition, WorldViewProjectionMatrix);
	result.normal = mul(input.normal, (float3x3)WorldMatrix);
	result.uv = input.uv;

    return result;
}

struct PSOutput
{
    float4 target0 : SV_Target0;
    float4 target1 : SV_Target1;
};

float4 GetWaves(float2 uv)
{
	uv = uv * 0.5;
	uv.y += Time / 2;
	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, uv);
	
	return albedo;
}

PSOutput PSMain(PSInput input)
{
	float4 albedo = GetWaves(input.uv);
	albedo.a = albedo.r;

	PSOutput output;
    output.target0 = albedo;
	output.target1 = albedo;

	return output;
}
