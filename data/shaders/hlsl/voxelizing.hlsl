#ifdef INSTANCED
float4x4 ViewProjectionMatrix;
#else
float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
#endif

float3 MaterialColor;
uint TexIdDiffuse;

float4x4 ShadowMatrix;
float3 SunDirection;
uint TexIdShadowOffset;
uint TexIdSceneVoxelBounces;

float Emission;

cbuffer SceneVoxelInfo : register(b1)
{
	float3 sceneCorner : packoffset(c0);
	float voxelDensity : packoffset(c0.w);
	float3 voxelSceneSize : packoffset(c1);
    float2 middleCone : packoffset(c2);
    float2 sideCone : packoffset(c2.z);
    float lerpFactor : packoffset(c3);
	float steppingBounces : packoffset(c3.y);
    float steppingDiffuse : packoffset(c3.z);
	float voxelizeLighting : packoffset(c3.w);
	float3 lastVoxelOffset : packoffset(c4);
};

#ifdef INSTANCED
StructuredBuffer<float4x4> InstancingBuffer : register(t0);
#endif

struct VS_Input
{
    float4 p : POSITION;
    float2 uv : TEXCOORD0;
    float3 n : NORMAL;
    float3 t : TANGENT;

#ifdef INSTANCED
	uint instanceID : SV_InstanceID;
#endif
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 wp : TEXCOORD1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
	
#ifdef INSTANCED
	uint instanceID : TEXCOORD2;
#endif
};

PS_Input VS_Main(VS_Input vin)
{
    PS_Input vsOut;

    vsOut.uv = vin.uv;
    vsOut.tangent = vin.t;
	vsOut.normal = vin.n;

#ifdef INSTANCED
	vsOut.wp = mul(vin.p, InstancingBuffer[vin.instanceID]);
	vsOut.instanceID = vin.instanceID;
#else
	vsOut.wp = mul(vin.p, WorldMatrix);
#endif

    vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);

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

SamplerState ShadowSampler : register(s2);

float readShadowmap(Texture2D shadowmap, float2 shadowCoord)
{
	return shadowmap.SampleLevel(ShadowSampler, shadowCoord, 0).r;
	
//	int2 texCoord = int2(shadowCoord * 512);
//	return shadowmap.Load(int3(texCoord, 0)).r;
}

float getShadow(float4 wp)
{
	Texture2D shadowmap = GetTexture(TexIdShadowOffset);
    float4 sunLookPos = mul(wp, ShadowMatrix);
    sunLookPos.xy = sunLookPos.xy / sunLookPos.w;
	sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;
	sunLookPos.z -= 0.01;

	return readShadowmap(shadowmap, sunLookPos.xy) < sunLookPos.z ? 0.0 : 1.0;
}

SamplerState LinearWrapSampler : register(s0);
SamplerState VoxelSampler : register(s1);
RWTexture3D<float4> SceneVoxel : register(u0);

float4 PS_Main(PS_Input pin) : SV_TARGET
{
#ifdef INSTANCED
	float3x3 worldMatrix = (float3x3)InstancingBuffer[pin.instanceID];
#else
	float3x3 worldMatrix = (float3x3)WorldMatrix;
#endif

	float3 normal = normalize(mul(pin.normal, worldMatrix).xyz);
	float3 binormal = cross(pin.normal, pin.tangent);


    float3 geometryNormal = normal;
    float3 geometryB = normalize(mul(binormal, worldMatrix).xyz);
    float3 geometryT = normalize(mul(pin.tangent, worldMatrix).xyz);

	float3 voxelUV = (pin.wp.xyz -sceneCorner) / voxelSceneSize;
	Texture3D SceneVoxelBounces = GetTexture3D(TexIdSceneVoxelBounces);
	float4 fullTraceSample = coneTrace(voxelUV, geometryNormal, middleCone.x, middleCone.y, SceneVoxelBounces, VoxelSampler, 0) * 1;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal + geometryT), sideCone.x, sideCone.y, SceneVoxelBounces, VoxelSampler, 0) * 1.0;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal - geometryT), sideCone.x, sideCone.y, SceneVoxelBounces, VoxelSampler, 0) * 1.0;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal + geometryB), sideCone.x, sideCone.y, SceneVoxelBounces, VoxelSampler, 0) * 1.0;
	fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal - geometryB), sideCone.x, sideCone.y, SceneVoxelBounces, VoxelSampler, 0) * 1.0;
    float3 traceColor = fullTraceSample.rgb;

	float3 diffuse = MaterialColor * GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, pin.uv).rgb;
	float3 baseColor = diffuse * traceColor * steppingBounces + diffuse * steppingDiffuse;

    float occlusionSample = sampleVox(SceneVoxelBounces, VoxelSampler, voxelUV + geometryNormal / 128, geometryNormal, 2).w;
    occlusionSample += sampleVox(SceneVoxelBounces, VoxelSampler, voxelUV + normalize(geometryNormal + geometryT) / 128, normalize(geometryNormal + geometryT), 2).w;
    occlusionSample += sampleVox(SceneVoxelBounces, VoxelSampler, voxelUV + normalize(geometryNormal - geometryT) / 128, normalize(geometryNormal - geometryT), 2).w;
    occlusionSample += sampleVox(SceneVoxelBounces, VoxelSampler, voxelUV + (geometryNormal + geometryB) / 128, (geometryNormal + geometryB), 2).w;
    occlusionSample += sampleVox(SceneVoxelBounces, VoxelSampler, voxelUV + (geometryNormal - geometryB) / 128, (geometryNormal - geometryB), 2).w;
    occlusionSample = 1 - saturate(occlusionSample / 5);
	baseColor *= occlusionSample;
	
	if (dot(SunDirection,normal) < 0)
		baseColor += diffuse * getShadow(pin.wp) * 0.5;

    float3 posUV = (pin.wp.xyz - sceneCorner) * voxelDensity;
	
	float3 prev = SceneVoxelBounces.Load(float4(posUV - lastVoxelOffset, 0)).rgb;
	baseColor.rgb = lerp(prev, baseColor.rgb, 0.1);

	baseColor = lerp(baseColor, diffuse * Emission, saturate(Emission));

    SceneVoxel[posUV] = float4(baseColor, (1 - saturate(Emission)));
	//SceneVoxel[posUV - normal].w = (1 - saturate(Emission));

	return float4(diffuse, 1);
}
