float4x4 WorldViewProjectionMatrix;
float4x4 WorldMatrix;
float4 MaterialColor;
float Time;
float2 ViewportSizeInverse;
uint TexIdColor;

cbuffer PSSMShadows : register(b1)
{
	float4x4 ShadowMatrix[2];
	float3 LightDirection;
	uint TexIdShadowOffset;
}

struct VSInput
{
    float4 position : POSITION;
	float3 normal : NORMAL;
    float2 uv : TEXCOORD;
	float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
	float3 normal : NORMAL;
    float4 color : COLOR;
	float2 uv : TEXCOORD1;
	float4 worldPosition : TEXCOORD2;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = mul(input.position, WorldViewProjectionMatrix);
    result.color = input.color;
	result.uv = input.uv;
	result.normal = input.normal;
	result.worldPosition = mul(input.position, WorldMatrix);

    return result;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState g_sampler : register(s0);

float readShadowmap(Texture2D shadowmap, float2 shadowCoord)
{
    int2 texCoord = int2(shadowCoord * 512);
	return shadowmap.Load(int3(texCoord, 0)).r;
	
	//return shadowmap.Sample(g_sampler, shadowCoord).r;
}

float ShadowPCF(Texture2D shadowmap, float4 shadowCoord)
{
    float shadow = 0.0;
    float2 texelSize = 1.0 / 512;
    int samples = 4; // Number of samples for PCF
    float2 offsets[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };

    for (int i = 0; i < samples; ++i)
    {
        float2 offset = offsets[i] * texelSize;
        shadow += readShadowmap(shadowmap, shadowCoord.xy + offset).r < shadowCoord.z ? 0.0 : 1.0;
    }

    return shadow / samples;
}

/*float FastShadowPCF(Texture2D shadowmap, float4 shadowCoord)
{
    float shadow = readShadowmap(shadowmap, shadowCoord.xy).r < shadowCoord.z ? 0.0 : 1.0;
	float4 samples = shadowmap.GatherCmpRed(g_sampler, shadowCoord.xyw, shadowCoord.z, int2(1,1));
    shadow = (shadow + samples.x + samples.y + samples.z + samples.w) * 0.2;

    return shadow;
}*/

float CalcShadowTermSoftPCF(Texture2D shadowmap, float fLightDepth, float2 vShadowTexCoord, int iSqrtSamples)
{
	float fShadowTerm = 0.0f;

	float fRadius = (iSqrtSamples - 1.0f) / 2;
	float fWeightAccum = 0.0f;

	for (float y = -fRadius; y <= fRadius; y++)
	{
		for (float x = -fRadius; x <= fRadius; x++)
		{
			float2 vOffset = 0;
			vOffset = float2(x, y);
			vOffset /= 512;
			float2 vSamplePoint = vShadowTexCoord + vOffset;
			float fDepth = readShadowmap(shadowmap, vSamplePoint).x;
			float fSample = (fLightDepth <= fDepth);

			// Edge tap smoothing
			float xWeight = 1;
			float yWeight = 1;
			if (x == -fRadius)
				xWeight = 1 - frac(vShadowTexCoord.x * 512);
			else if (x == fRadius)
				xWeight = frac(vShadowTexCoord.x * 512);
			if (y == -fRadius)
				yWeight = 1 - frac(vShadowTexCoord.y * 512);
			else if (y == fRadius)
				yWeight = frac(vShadowTexCoord.y * 512);
			fShadowTerm += fSample * xWeight * yWeight;
			fWeightAccum = xWeight * yWeight;
		}
	}
	fShadowTerm /= (iSqrtSamples * iSqrtSamples);
	fShadowTerm *= 1.55f;

	return fShadowTerm;
}

float getShadow(float4 wp)
{
	uint ShadowIndex = 0;

	Texture2D shadowmap = GetTexture(TexIdShadowOffset + ShadowIndex);
    float4 sunLookPos = mul(wp, ShadowMatrix[ShadowIndex]);
    sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;

	sunLookPos.z -= 0.01;

    // Convert shadow coordinates to texture coordinates
    //int2 texCoord = int2(shadowCoord * 512);
    //float shadowValue = shadowmap.Load(int3(texCoord, 0)).r;

    //return sunLookPos.z - 0.01 < shadowValue;
	
	//return FastShadowPCF(shadowmap, sunLookPos);
	//return ShadowPCF(shadowmap, sunLookPos);
	
	return CalcShadowTermSoftPCF(shadowmap, sunLookPos.z, sunLookPos.xy, 5);
}

struct PSOutput
{
    float4 target0 : SV_Target0;
    float4 target1 : SV_Target1;
};

PSOutput PSMain(PSInput input)
{
	float3 normal = normalize(mul(input.normal, (float3x3)WorldMatrix).xyz);
	float diffuse = saturate(dot(-LightDirection, normal));

	float2 screenCoords = input.position.xy;
    float2 normalizedCoords = screenCoords * ViewportSizeInverse;

	//input.color * lerp(GetTexture(TexIdGrass).Sample(g_sampler, normalizedCoords * 2), GetTexture(TexIdColor).Sample(g_sampler, input.uv), abs(sin(Time)))
	
	PSOutput output;
    output.target0 = (input.color + getShadow(input.worldPosition) * diffuse) * GetTexture(TexIdColor).Sample(g_sampler, input.uv) * MaterialColor;
	output.target1 = input.color;

	return output;
}
