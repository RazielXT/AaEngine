struct SunParams
{
	float4x4 ShadowMatrix[4];
	float4x4 MaxShadowMatrix;
	float3 Direction;
	uint TexIdShadowOffset;
	float3 Color;
	float ShadowMapSize;
	float ShadowMapSizeInv;
	float ShadowCascadeDistance0;
	float ShadowCascadeDistance1;
	float ShadowCascadeDistance2;
};

float readShadowmap(Texture2D<float> shadowmap, SamplerState sampler, float2 shadowCoord)
{
	return shadowmap.SampleLevel(sampler, shadowCoord, 0).r;
	
//	int2 texCoord = int2(shadowCoord * 512);
//	return shadowmap.Load(int3(texCoord, 0)).r;
}

float CalcShadowTermSoftPCF(Texture2D<float> shadowmap, SamplerState sampler, float fLightDepth, float2 vShadowTexCoord, int iSqrtSamples, float ShadowMapSize, float ShadowMapSizeInv)
{
	float fShadowTerm = 0.0f;

	float fRadius = (iSqrtSamples - 1.0f) / 2;
	float fWeightAccum = 0.0f;

	if (fLightDepth >= 1)
		fLightDepth = 0;

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

float CalculatePCFPercentLit(Texture2D<float> shadowmap, SamplerComparisonState sampler, float fLightDepth, float2 vShadowTexCoord, float ShadowMapSizeInv) 
{
    float fPercentLit = 0.0f;
    // This loop could be unrolled, and texture immediate offsets could be used if the kernel size were fixed.
    // This would be performance improvment.
	int m_iPCFBlurForLoopStart = -2;
	int m_iPCFBlurForLoopEnd = 3;

    for( int x = m_iPCFBlurForLoopStart; x < m_iPCFBlurForLoopEnd; ++x ) 
    {
        for( int y = m_iPCFBlurForLoopStart; y < m_iPCFBlurForLoopEnd; ++y ) 
        {
            fPercentLit += shadowmap.SampleCmpLevelZero(sampler, 
                float2( 
                    vShadowTexCoord.x + ( ( (float) x ) * ShadowMapSizeInv / 3 ) , 
                    vShadowTexCoord.y + ( ( (float) y ) * ShadowMapSizeInv ) 
                    ), 
                fLightDepth);
        }
    }
    fPercentLit /= (float)5*5;
	
	return fPercentLit;
}

float ShadowPCF(Texture2D<float> shadowmap, SamplerState sampler, float4 shadowCoord, float ShadowMapSizeInv)
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
