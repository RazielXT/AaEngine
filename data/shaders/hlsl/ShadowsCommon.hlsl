struct SunParams
{
	float4x4 ShadowMatrix[2];
	float3 Direction;
	uint TexIdShadowOffset;
	float3 Color;
	float ShadowMapSize;
	float ShadowMapSizeInv;
};

float readShadowmap(Texture2D shadowmap, SamplerState sampler, float2 shadowCoord)
{
	return shadowmap.SampleLevel(sampler, shadowCoord, 0).r;
	
//	int2 texCoord = int2(shadowCoord * 512);
//	return shadowmap.Load(int3(texCoord, 0)).r;
}

float CalcShadowTermSoftPCF(Texture2D shadowmap, SamplerState sampler, float fLightDepth, float2 vShadowTexCoord, int iSqrtSamples, float ShadowMapSize, float ShadowMapSizeInv)
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
			float fDepth = readShadowmap(shadowmap, sampler, vSamplePoint);
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

float ShadowPCF(Texture2D shadowmap, SamplerState sampler, float4 shadowCoord, float ShadowMapSizeInv)
{
    float shadow = 0.0;
    float2 texelSize = ShadowMapSizeInv;
    int samples = 4; // Number of samples for PCF
    float2 offsets[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };

    for (int i = 0; i < samples; ++i)
    {
        float2 offset = offsets[i] * texelSize;
        shadow += readShadowmap(shadowmap, sampler, shadowCoord.xy + offset).r < shadowCoord.z ? 0.0 : 1.0;
    }

    return shadow / samples;
}
