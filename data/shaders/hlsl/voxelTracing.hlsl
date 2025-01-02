float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float4x4 PreviousWorldMatrix;
float3 CameraPosition;
float Emission;
float3 MaterialColor;
uint TexIdDiffuse;
uint TexIdSceneVoxel;
uint2 ViewportSize;

cbuffer PSSMShadows : register(b1)
{
	float4x4 ShadowMatrix[2];
	float3 SunDirection;
	uint TexIdShadowOffset;
	float3 LightColor;
	float ShadowMapSize;
	float ShadowMapSizeInv;
}

cbuffer SceneVoxelInfo : register(b2)
{
	float3 sceneCorner : packoffset(c0);
	float voxelDensity : packoffset(c0.w);
	float3 voxelSceneSize : packoffset(c1);
    float2 middleCone : packoffset(c2);
    float2 sideCone : packoffset(c2.z);
};

struct VS_Input
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float3 t : TANGENT;
    float2 uv : TEXCOORD0;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 wp : TEXCOORD1;
	float4 currentPosition : TEXCOORD2;
	float4 previousPosition : TEXCOORD3;
};

PS_Input VSMain(VS_Input vin)
{
    PS_Input vsOut;

    vsOut.wp = mul(vin.p, WorldMatrix);
    vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
    vsOut.uv = vin.uv;
    vsOut.normal = vin.n;
    vsOut.tangent = vin.t;
	
	float4 previousWorldPosition = mul(vin.p, PreviousWorldMatrix);
	vsOut.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	vsOut.currentPosition = vsOut.pos;

    return vsOut;
}

float4 sampleVox(Texture3D cmap, SamplerState sampl, float3 pos, float3 dir, float lod)
{
	float voxDim = 128.0f;
    float sampOff = 1;

    float4 sample0 = cmap.SampleLevel(sampl, pos, lod);
    float4 sampleX = dir.x < 0.0 ? cmap.SampleLevel(sampl, pos - float3(sampOff, 0, 0) / voxDim, lod) : cmap.SampleLevel(sampl, pos + float3(sampOff, 0, 0) / voxDim, lod);
    float4 sampleY = dir.y < 0.0 ? cmap.SampleLevel(sampl, pos - float3(0, sampOff, 0) / voxDim, lod) : cmap.SampleLevel(sampl, pos + float3(0, sampOff, 0) / voxDim, lod);
    float4 sampleZ = dir.z < 0.0 ? cmap.SampleLevel(sampl, pos - float3(0, 0, sampOff) / voxDim, lod) : cmap.SampleLevel(sampl, pos + float3(0, 0, sampOff) / voxDim, lod);

    float3 wts = abs(dir);

    float invSampleMag = 1.0 / (wts.x + wts.y + wts.z + 0.0001);
    wts *= invSampleMag;

    float4 filtered;
    filtered.xyz = (sampleX.xyz * wts.x + sampleY.xyz * wts.y + sampleZ.xyz * wts.z);
    filtered.w = (sampleX.w * wts.x + sampleY.w * wts.y + sampleZ.w * wts.z);

    return filtered;
}

float4 coneTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl, float closeZero)
{
	float voxDim = 128.0f;
    float3 samplePos = o;
    float4 accum = float4(0, 0, 0, 0);
    float minDiam = 1.0 / voxDim;
    float startDist = minDiam * 3;
    float dist = startDist;

    closeZero *= 2;
    closeZero += 1;

    while (dist <= maxDist && accum.w < 1.0)
    {
        float sampleDiam = max(minDiam, coneRatio * dist);
        float sampleLOD = max(0, log2(sampleDiam * voxDim));
        float3 samplePos = o + d * dist;
        float4 sampleVal = sampleVox(voxelTexture, sampl, samplePos, -d, sampleLOD);

        float lodChange = max(0, sampleLOD * sampleLOD / closeZero);
        float insideVal = sampleVal.w;
        sampleVal.rgb *= lodChange;

        lodChange = max(0, sampleLOD * closeZero);
        sampleVal.w = saturate(lodChange * insideVal);

        float sampleWt = (1.0 - accum.w);
        accum += sampleVal * sampleWt;

        dist += sampleDiam;
    }

    accum.xyz *= accum.w;

    return accum;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
Texture3D<float4> GetTexture3D(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState ShadowSampler : register(s1);

float readShadowmap(Texture2D shadowmap, float2 shadowCoord)
{
	return shadowmap.SampleLevel(ShadowSampler, shadowCoord, 0).r;
	
//	int2 texCoord = int2(shadowCoord * 512);
//	return shadowmap.Load(int3(texCoord, 0)).r;
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
			float fSample = (fLightDepth < fDepth);

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
	Texture2D shadowmap = GetTexture(TexIdShadowOffset);
    float4 sunLookPos = mul(wp, ShadowMatrix[0]);
    sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;
	sunLookPos.z -= 0.01;

	//return readShadowmap(shadowmap, sunLookPos.xy) < sunLookPos.z ? 0.0 : 1.0;
	return CalcShadowTermSoftPCF(shadowmap, sunLookPos.z, sunLookPos.xy, 5);
}

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

SamplerState VoxelSampler : register(s0);

PSOutput PSMain(PS_Input pin)
{
	float3 binormal = cross(pin.normal, pin.tangent);
	float3 normal = normalize(mul(pin.normal, (float3x3) WorldMatrix).xyz);

    float3 geometryNormal = normal;
    float3 geometryB = normalize(mul(binormal, (float3x3) WorldMatrix).xyz);
    float3 geometryT = normalize(mul(pin.tangent, (float3x3) WorldMatrix).xyz);

    float3 voxelUV = (pin.wp.xyz-sceneCorner)/voxelSceneSize;
	Texture3D voxelmap = GetTexture3D(TexIdSceneVoxel);
    float4 fullTraceSample = coneTrace(voxelUV, geometryNormal, middleCone.x, middleCone.y, voxelmap, VoxelSampler, 0) * 1;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal + geometryT), sideCone.x, sideCone.y, voxelmap, VoxelSampler, 0) * 1.0;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal - geometryT), sideCone.x, sideCone.y, voxelmap, VoxelSampler, 0) * 1.0;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal + geometryB), sideCone.x, sideCone.y, voxelmap, VoxelSampler, 0) * 1.0;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal - geometryB), sideCone.x, sideCone.y, voxelmap, VoxelSampler, 0) * 1.0;
    float3 traceColor = fullTraceSample.rgb / 5;

    float occlusionSample = sampleVox(voxelmap, VoxelSampler, voxelUV + geometryNormal / 128, geometryNormal, 2).w;
    occlusionSample += sampleVox(voxelmap, VoxelSampler, voxelUV + normalize(geometryNormal + geometryT) / 128, normalize(geometryNormal + geometryT), 2).w;
    occlusionSample += sampleVox(voxelmap, VoxelSampler, voxelUV + normalize(geometryNormal - geometryT) / 128, normalize(geometryNormal - geometryT), 2).w;
    occlusionSample += sampleVox(voxelmap, VoxelSampler, voxelUV + (geometryNormal + geometryB) / 128, (geometryNormal + geometryB), 2).w;
    occlusionSample += sampleVox(voxelmap, VoxelSampler, voxelUV + (geometryNormal - geometryB) / 128, (geometryNormal - geometryB), 2).w;
    occlusionSample = 1 - saturate(occlusionSample / 5);

	float3 voxelAmbient = traceColor * occlusionSample;
	
	float camDistance = length(CameraPosition - pin.wp.xyz);
	float3 staticAmbient = float3(0.2, 0.2, 0.2);

	float voxelWeight = saturate(-0.5 + camDistance / (voxelSceneSize.x * 0.5));
	float3 finalAmbient = lerp(voxelAmbient, staticAmbient, voxelWeight);
	
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];
    float3 albedo = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, pin.uv / 40).rgb;
	albedo *= MaterialColor;

	float directShadow = getShadow(pin.wp);
	float directLight = saturate(dot(-SunDirection,normal)) * directShadow;
	float3 lighting = max(directLight, finalAmbient);

    float4 color1 = float4(saturate(albedo * lighting), 1);
	color1.rgb += albedo * Emission;
	color1.a = (lighting.r + lighting.g + lighting.b) / 3;
	//color1.rgb = voxelmap.SampleLevel(VoxelSampler, voxelUV, 0).rgb;

	PSOutput output;
    output.color = color1;
	output.normals = float4(normal, 1);
	

	output.camDistance = float4(camDistance / 10000, 0, 0, 0);

	output.motionVectors = float4((pin.previousPosition.xy / pin.previousPosition.w - pin.currentPosition.xy / pin.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;

	return output;
}
