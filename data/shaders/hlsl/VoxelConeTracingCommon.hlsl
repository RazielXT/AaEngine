struct SceneVoxelChunkInfo
{
    float3 Offset;
    float Density;
    float3 BouncesOffset;
    float SceneSize;
    float LerpFactor;
    uint TexId;
    uint TexIdBounces;
	uint ResIdData;
};

struct SceneVoxelCbuffer
{
    float2 MiddleConeRatio;
    float2 SideConeRatio;
    float SteppingBounces;
    float SteppingDiffuse;
    float VoxelizeLighting;
    float Padding;

    SceneVoxelChunkInfo NearVoxels;
    SceneVoxelChunkInfo FarVoxels;
};

struct SceneVoxelCbufferIndexed
{
    float2 MiddleConeRatio;
    float2 SideConeRatio;
    float SteppingBounces;
    float SteppingDiffuse;
    float VoxelizeLighting;
    float Padding;

    SceneVoxelChunkInfo Voxels[4];
};

float4 SampleVoxel(Texture3D cmap, SamplerState sampl, float3 pos, float3 dir, float lod)
{
#ifndef GI_MAX
	return cmap.SampleLevel(sampl, pos, lod);
#endif

	const float voxDim = 128.0f;
    const float sampOff = 1;

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

float4 ConeTraceImpl(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl, float lodOffset)
{
	const float voxDim = 128.0f;
    const float minDiam = 1.0 / voxDim;
    const float startDist = minDiam * 2;
    float dist = startDist;
    float3 samplePos = o;
    float4 accum = float4(0, 0, 0, 0);

    while (dist <= maxDist && accum.w < 1.0)
    {
        float sampleDiam = max(minDiam, coneRatio * dist);
        float sampleLOD = max(0, log2(sampleDiam * voxDim) + lodOffset);
        float3 samplePos = o + d * dist;
        float4 sampleVal = SampleVoxel(voxelTexture, sampl, samplePos, -d, sampleLOD);

        float lodChange = max(0, sampleLOD * sampleLOD);
        float insideVal = sampleVal.w;
        sampleVal.rgb *= lodChange;

        lodChange = max(0, sampleLOD);
        sampleVal.w = saturate(lodChange * insideVal);

        float sampleWt = (1.0 - accum.w);
        accum += sampleVal * sampleWt;

        dist += sampleDiam;
    }

    accum.xyz *= accum.w;

    return accum;
}


float4 ConeTraceNear(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl)
{
	return ConeTraceImpl(o, d, coneRatio, maxDist, voxelTexture, sampl, -1);
}

float4 ConeTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl)
{
	return ConeTraceImpl(o, d, coneRatio, maxDist, voxelTexture, sampl, 0);
}

float4 DebugConeTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl, int idx)
{
	int i = 0;
	const float voxDim = 128.0f;
    const float minDiam = 1.0 / voxDim;
    const float startDist = minDiam * 3;
    float dist = startDist;
    float3 samplePos = o;
    float4 accum = float4(0, 0, 0, 0);

    while (dist <= maxDist && accum.w < 1.0)
    {
        float sampleDiam = max(minDiam, coneRatio * dist);
        float sampleLOD = max(0, log2(sampleDiam * voxDim) - 1);
        float3 samplePos = o + d * dist;
        float4 sampleVal = SampleVoxel(voxelTexture, sampl, samplePos, -d, sampleLOD);

        float lodChange = max(0, sampleLOD * sampleLOD);
        float insideVal = sampleVal.w;
        sampleVal.rgb *= lodChange;

        lodChange = max(0, sampleLOD);
        sampleVal.w = saturate(lodChange * insideVal);


		if (i == idx)
			return sampleVal;

		idx++;

        float sampleWt = (1.0 - accum.w);
        accum += sampleVal * sampleWt;

        dist += sampleDiam;
    }

    accum.xyz *= accum.w;

    return accum;
}

struct VoxelSceneData
{
	float4 Diffuse;
	float3 Normal;
	int Max;
	float Occupy;
};
