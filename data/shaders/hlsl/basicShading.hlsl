float4x4 ViewProjectionMatrix;
float4x4 PreviousWorldMatrix;
float4x4 WorldMatrix;
float3 MaterialColor;
uint TexIdDiffuse;
float3 CameraPosition;
uint2 ViewportSize;

cbuffer PSSMShadows : register(b1)
{
	float4x4 ShadowMatrix[2];
	float3 LightDirection;
	uint TexIdShadowOffset;
	float3 LightColor;
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
	float4 previousPosition : TEXCOORD3;
	float4 currentPosition : TEXCOORD4;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

#ifdef INSTANCED
	result.worldPosition = mul(input.position, InstancingBuffer[input.instanceID]);
    result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = mul(input.normal, (float3x3)InstancingBuffer[input.instanceID]);
	result.previousPosition = result.position; // no support
	result.currentPosition = result.position;
#else
	result.worldPosition = mul(input.position, WorldMatrix);
    result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = mul(input.normal, (float3x3)WorldMatrix);
	float4 previousWorldPosition = mul(input.position, PreviousWorldMatrix);
	result.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	result.currentPosition = result.position;
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

SamplerState ShadowSampler : register(s0);

float readShadowmap(Texture2D shadowmap, float2 shadowCoord)
{
	return shadowmap.SampleLevel(ShadowSampler, shadowCoord, 0).r;
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
			float fSample = (fLightDepth < fDepth);

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
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

PSOutput PSMain(PSInput input)
{
	float3 normal = input.normal;//normalize(mul(input.normal, (float3x3)WorldMatrix).xyz);

	float3 diffuse = saturate(dot(-LightDirection, normal)) * LightColor;

#ifdef USE_VC
	float3 ambientColor = input.color.rgb;
#else
	float3 ambientColor = float3(0.2,0.2,0.2);
#endif

	SamplerState sampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(sampler, input.uv);
	albedo.rgb *= MaterialColor;

	float3 lighting = (ambientColor + getShadow(input.worldPosition) * diffuse);
	float lightPower = (lighting.r + lighting.g + lighting.b) / 3;
	float4 outColor = float4(lighting * albedo.rgb, lightPower);
	
	
	PSOutput output;
    output.color = outColor;
	output.normals = float4(normal, 1);
	float camDistance = length(CameraPosition - input.worldPosition.xyz) / 10000;
	output.camDistance = float4(camDistance, 0, 0, 0);
	output.motionVectors = float4((input.previousPosition.xy / input.previousPosition.w - input.currentPosition.xy / input.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;
	return output;
}
