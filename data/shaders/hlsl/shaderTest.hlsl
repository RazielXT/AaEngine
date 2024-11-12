float4x4 WorldViewProjectionMatrix;
float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float3 MaterialColor;
uint TexIdDiffuse;

cbuffer PSSMShadows : register(b1)
{
	float4x4 ShadowMatrix[2];
	float3 LightDirection;
	uint TexIdShadowOffset;
	float ShadowMapSize;
	float ShadowMapSizeInv;
}

#ifdef INSTANCED
StructuredBuffer<float4x4> InstancingBuffer : register(t0);
#endif

struct VSInput
{
    float4 position : POSITION;
	float3 normal : NORMAL;
    float2 uv : TEXCOORD;
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
	float2 uv : TEXCOORD1;
	float4 worldPosition : TEXCOORD2;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

#ifdef INSTANCED
	result.worldPosition = mul(input.position, InstancingBuffer[input.instanceID]);
    result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = mul(input.normal, (float3x3)InstancingBuffer[input.instanceID]);
#else
	result.worldPosition = mul(input.position, WorldMatrix);
    result.position = mul(input.position, WorldViewProjectionMatrix);
	result.normal = mul(input.normal, (float3x3)WorldMatrix);
#endif

#ifdef USE_VC
    result.color = input.color;
#endif

	result.uv = input.uv;

    return result;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState g_sampler : register(s0);
SamplerState DepthSampler : register(s1);

float readShadowmap(Texture2D shadowmap, float2 shadowCoord)
{
	return shadowmap.SampleLevel(DepthSampler, shadowCoord, 0).r;
}

float ShadowPCF(Texture2D shadowmap, float4 shadowCoord)
{
    float shadow = 0.0;
    float2 texelSize = ShadowMapSizeInv;
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
			vOffset *= ShadowMapSizeInv;
			float2 vSamplePoint = vShadowTexCoord + vOffset;
			float fDepth = readShadowmap(shadowmap, vSamplePoint).x;
			float fSample = (fLightDepth <= fDepth);

			// Edge tap smoothing
			float xWeight = 1;
			float yWeight = 1;
			if (x == -fRadius)
				xWeight = 1 - frac(vShadowTexCoord.x * ShadowMapSize);
			else if (x == fRadius)
				xWeight = frac(vShadowTexCoord.x * ShadowMapSize);
			if (y == -fRadius)
				yWeight = 1 - frac(vShadowTexCoord.y * ShadowMapSize);
			else if (y == fRadius)
				yWeight = frac(vShadowTexCoord.y * ShadowMapSize);
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
	//float3 normal = normalize(mul(input.normal, (float3x3)WorldMatrix).xyz);

	float diffuse = saturate(dot(-LightDirection, input.normal));

#ifdef USE_VC
	float4 ambientColor = input.color;
#else
	float4 ambientColor = float4(0.2,0.2,0.2,1);
#endif

	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, input.uv);
	albedo.rgb *= MaterialColor;

	PSOutput output;
    output.target0 = (ambientColor + getShadow(input.worldPosition) * diffuse) * albedo;
	output.target1 = output.target0 * output.target0 * 2;

	return output;
}
