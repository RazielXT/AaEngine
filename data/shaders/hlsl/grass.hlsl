float4x4 ViewProjectionMatrix;
uint TexIdDiffuse;
float Time;

cbuffer PSSMShadows : register(b1)
{
	float4x4 ShadowMatrix[2];
	float3 LightDirection;
	uint TexIdShadowOffset;
}

struct GrassVertex { float3 pos; float3 color; };

StructuredBuffer<GrassVertex> GeometryBuffer : register(t0);

struct PSInput
{
    float4 position : SV_POSITION;
	float2 uv : TEXCOORD1;
	float3 color : TEXCOORD2;
	float4 worldPosition : TEXCOORD3;
};

static const float2 coords[4] = {
    float2(1.0f, 1.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),	
    float2(0.0f, 0.0f)
};

static const uint indices[6] = {
    0, 1, 2, // First triangle
    2, 1, 3  // Second triangle
};

PSInput VSMain(uint vertexIdx : SV_VertexId)
{
    PSInput output;

	uint quadID = vertexIdx / 6;
    uint vertexID = vertexIdx % 6;
	float2 uv = coords[indices[vertexID]];

	GrassVertex v = GeometryBuffer[quadID * 2 + uv.x];
	float3 pos = v.pos;
	
	float top = 1 - uv.y;
	float grassHeight = 4;
	pos.y += top * grassHeight;

	float3 windDirection = float3(1,0,1);
	float frequency = 1;
	pos.xyz += top * windDirection * sin(Time * frequency + pos.x * pos.z + pos.y);

    // Output position and UV
	output.worldPosition = float4(pos,1);
    output.position = mul(output.worldPosition, ViewProjectionMatrix);
    output.uv = uv;
	output.color = v.color;

    return output;
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
}

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

	return CalcShadowTermSoftPCF(shadowmap, sunLookPos.z, sunLookPos.xy, 3);
}

struct PSOutput
{
    float4 target0 : SV_Target0;
    float4 target1 : SV_Target1;
};

PSOutput PSMain(PSInput input)
{
	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, input.uv);

	float distanceFade = saturate(200 * input.position.z / input.position.w);

	if (albedo.a * distanceFade <0.5) discard;

	albedo.rgb *= input.color * 2;

	float shadowing = getShadow(input.worldPosition);
	float shading = saturate(1.7 - (1 - shadowing) * 0.5 - input.uv.y) * (0.5 + 0.5 * shadowing);

	PSOutput output;
    output.target0 = albedo * distanceFade * shading;
	output.target1 = albedo * albedo;

	return output;
}

void PSMainDepth(PSInput input)
{
	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, input.uv);

	float distanceFade = saturate(200 * input.position.z / input.position.w);

	if (albedo.a * distanceFade <0.5) discard;
}
