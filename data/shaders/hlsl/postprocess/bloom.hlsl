#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D colorMap : register(t0);
Texture2D brightnessMap : register(t1);
SamplerState LinearSampler : register(s0);

static const float bloomSampleWeight[11] =
{
    0.01023664f,
    0.03086506f,
    0.04888703f,
    0.09208646f,
    0.14829802f,

    0.33939156f,

    0.14829802f,
    0.09208646f,
    0.04888703f,
    0.03086506f,
    0.01023664f
};

static const float bloomOffset[11] =
{
	-1.5,
	-1.1,
	-0.7,
	-0.4,
	-0.25,
	 0,
	 0.25,
	 0.4,
	 0.7,
	 1.1,
	 1.5
};

#define bloomRadius 6

float4 PSBloomBlurX(VS_OUTPUT input) : SV_TARGET
{
	float brightness = 1 - brightnessMap.Load(int3(0,0,0)).r;
	float bloomSize = brightness * bloomRadius + 0.1;
	float2 offsetSize = ViewportSizeInverse * bloomSize;

    float4 output = 0;
	for (int i = 0; i < 11; i++)
	{
		output += colorMap.Sample(LinearSampler, input.TexCoord + offsetSize * float2(bloomOffset[i],0)) * bloomSampleWeight[i];
	}

	return output;
}

float4 PSBloomBlurY(VS_OUTPUT input) : SV_TARGET
{
	float brightness = 1 - brightnessMap.Load(int3(0,0,0)).r;
	float bloomSize = brightness * bloomRadius + 0.1;
	float2 offsetSize = ViewportSizeInverse * bloomSize;

    float4 output = 0;
	for (int i = 0; i < 11; i++)
	{
		output += colorMap.Sample(LinearSampler, input.TexCoord + offsetSize * float2(0,bloomOffset[i])) * bloomSampleWeight[i];
	}

	return output;
}
