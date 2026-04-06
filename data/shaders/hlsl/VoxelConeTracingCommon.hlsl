struct SceneVoxelChunkInfo
{
	float3 Offset;
	float Density;
	float3 MoveOffset;
	float WorldSize;
	uint TexId;
	uint TexIdPrev;
	uint ResIdData;
};

struct SceneVoxelCbuffer
{
	float2 MiddleConeRatio;
	float2 SideConeRatio;

	SceneVoxelChunkInfo NearVoxels;
	SceneVoxelChunkInfo FarVoxels;
};

struct SceneVoxelCbufferIndexed
{
	float2 MiddleConeRatio;
	float2 SideConeRatio;

	SceneVoxelChunkInfo Voxels[4];
};

float4 SampleVoxel(Texture3D cmap, SamplerState sampl, float3 pos, float lod)
{
	return cmap.SampleLevel(sampl, pos, lod);
}

float4 ConeTraceImpl(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl)
{
	const float voxDim = 128.0f;
	const float minDiam = 1.0 / voxDim;
	const float startDist = minDiam;
	float dist = startDist;
	float3 samplePos = o;
	float4 accum = float4(0, 0, 0, 0);

	[loop]
	while (dist <= maxDist && accum.w < 1.0)
	{
		float sampleDiam = max(minDiam, coneRatio * dist);
		float sampleLOD = max(0, log2(sampleDiam * voxDim));
		float3 samplePos = o + d * dist;
		float4 voxel = SampleVoxel(voxelTexture, sampl, samplePos, sampleLOD);

		float sampleWt = (1.0 - accum.w);
		accum += voxel * sampleWt;

		dist += sampleDiam * 0.5;
	}

	accum.xyz *= accum.w;

	return accum;
}


float4 ConeTraceNear(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl)
{
	return ConeTraceImpl(o, d, coneRatio, maxDist, voxelTexture, sampl);//, -1);
}

float4 ConeTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl)
{
	return ConeTraceImpl(o, d, coneRatio, maxDist, voxelTexture, sampl);
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
		float4 sampleVal = SampleVoxel(voxelTexture, sampl, samplePos, sampleLOD);

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
	uint Diffuse;
	uint Normal;
};

// Pack float4 [-1..1] into signed R10G10B10A2 (SNORM)
uint PackRGB10A2_SNORM(float4 c)
{
    // Clamp to SNORM range
    c = clamp(c, -1.0, 1.0);

    // Convert to signed integers
    int r = (int)(c.r * 511.0 + (c.r >= 0 ? 0.5 : -0.5));   // 10-bit signed
    int g = (int)(c.g * 511.0 + (c.g >= 0 ? 0.5 : -0.5));
    int b = (int)(c.b * 511.0 + (c.b >= 0 ? 0.5 : -0.5));
    int a = (int)(c.a *   1.0 + (c.a >= 0 ? 0.5 : -0.5));   // 2-bit signed

    // Reinterpret as unsigned for packing
    uint ur = (uint)(r & 0x3FF);
    uint ug = (uint)(g & 0x3FF);
    uint ub = (uint)(b & 0x3FF);
    uint ua = (uint)(a & 0x003);

    return ur | (ug << 10) | (ub << 20) | (ua << 30);
}

// Unpack signed R10G10B10A2_SNORM back into float4 [-1..1]
float4 UnpackRGB10A2_SNORM(uint v)
{
    // Extract raw bits
    int r = (int)(v        & 0x3FF);
    int g = (int)((v >> 10) & 0x3FF);
    int b = (int)((v >> 20) & 0x3FF);
    int a = (int)((v >> 30) & 0x003);

    // Sign‑extend 10‑bit and 2‑bit values
    r = (r << 22) >> 22;   // 10‑bit sign extend
    g = (g << 22) >> 22;
    b = (b << 22) >> 22;
    a = (a << 30) >> 30;   // 2‑bit sign extend

    float4 c;
    c.r = r / 511.0;
    c.g = g / 511.0;
    c.b = b / 511.0;
    c.a = a /   1.0;
    return c;
}

// Pack float4 [0..1] into RGBA8 uint
uint PackRGBA8(float4 c)
{
    uint4 u = (uint4)(saturate(c) * 255.0 + 0.5);
    return (u.r) | (u.g << 8) | (u.b << 16) | (u.a << 24);
}

// Unpack RGBA8 uint back into float4 [0..1]
float4 UnpackRGBA8(uint v)
{
    float4 c;
    c.r = (v        & 0xFF) / 255.0;
    c.g = ((v >> 8) & 0xFF) / 255.0;
    c.b = ((v >> 16) & 0xFF) / 255.0;
    c.a = ((v >> 24) & 0xFF) / 255.0;
    return c;
}
