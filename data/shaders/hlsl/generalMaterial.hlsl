cbuffer PerFrame : register(b0)
{
    float4x4 sun_vpm : packoffset(c4);
    float3 camPos : packoffset(c8);
    float3 ambient : packoffset(c9);
    float3 sunDir : packoffset(c10);
    float3 sunColor : packoffset(c11);
};

cbuffer PerObject : register(b1)
{
    float4x4 wvpMat : packoffset(c0);
    float4x4 wMat : packoffset(c4);
    float3 worldPos : packoffset(c8);
};

cbuffer SceneVoxelInfo : register(b3)
{
    float2 middleCone : packoffset(c0);
    float2 sideCone : packoffset(c0.z);
    float radius : packoffset(c1);
	float3 voxelOffset : packoffset(c2);
	float voxelSize : packoffset(c2.w);
};

struct VS_Input
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float3 t : TANGENT;
    float2 uv : TEXCOORD0;
#ifdef USE_VC
	float3 color : COLOR;
#endif
};

struct VS_Out
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 wp : TEXCOORD1;
#ifdef USE_VC
	float3 color : TEXCOORD2;
#endif
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 wp : TEXCOORD1;
#ifdef USE_VC
	float3 color : TEXCOORD2;
#endif
};

VS_Out VS_Main(VS_Input vin)
{
    VS_Out vsOut = (VS_Out) 0;

    vsOut.pos = mul(vin.p, wvpMat);
    vsOut.uv = vin.uv * float2(1, -1);
    vsOut.normal = vin.n;
    vsOut.tangent = vin.t;
    vsOut.wp = mul(vin.p, wMat);
	
#ifdef USE_VC
	vsOut.color = vin.color;
#endif

    return vsOut;
}

float vsmShadow(Texture2D shadowmap, SamplerState colorSampler, float4 wp, float4x4 sun_vpm)
{
    float4 sunLookPos = mul(wp, transpose(sun_vpm));
    sunLookPos.xyz /= sunLookPos.w;
    sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;

    float2 moments = float2(0, 0);

    int steps = 3;
    float stepSize = 0.001f;

    for (int x = 0; x < steps; ++x)
        for (int y = 0; y < steps; ++y)
            moments += shadowmap.Sample(colorSampler, float2(sunLookPos.xy + float2(x * stepSize, y * stepSize))).xy;

    moments /= steps * steps;

    float ourDepth = sunLookPos.z;
    float litFactor = (ourDepth <= moments.x ? 1 : 0);
	// standard variance shadow mapping code
    float E_x2 = moments.y;
    float Ex_2 = moments.x * moments.x;
    float vsmEpsilon = 0.000005;
    float variance = min(max(E_x2 - Ex_2, 0.0) + vsmEpsilon, 1.0);
    float m_d = moments.x - ourDepth;
    float p = variance / (variance + m_d * m_d);

    return smoothstep(0.5, 1, max(litFactor, p));
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

float4 coneTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D voxelTexture, SamplerState sampl, float closeZero, float radius)
{
	float voxDim = 128.0f;
    float3 samplePos = o;
    float4 accum = float4(0, 0, 0, 0);
    float minDiam = 1.0 / voxDim;
    float startDist = minDiam * 2;
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

float4 PS_Main(PS_Input pin,
	Texture2D colorMap : register(t0),
	SamplerState colorSampler : register(s0),
	Texture2D normalmap : register(t1),
	SamplerState colorSampler1 : register(s1),
	Texture2D shadowmap : register(t2),
	SamplerState colorSampler2 : register(s2),
	Texture3D voxelmap : register(t3),
	SamplerState colorSampler3 : register(s3)
) : SV_TARGET
{
    float3 nSunDir = normalize(sunDir);

    float3 normalTex = normalmap.Sample(colorSampler1, pin.uv).rgb;
    normalTex = (normalTex + float3(0.5f, 0.5f, 1.0f) * 3) / 4;
    float3 bin = cross(pin.normal, pin.tangent);
    float3x3 tbn = float3x3(pin.tangent, bin, pin.normal);
    float3 normal = mul(transpose(tbn), normalTex.xyz * 2 - 1); // to object space
    normal = normalize(mul(normal, (float3x3) wMat).xyz);

    float diffuse = max(0, dot(-nSunDir, normal)).x;
    float shadow = vsmShadow(shadowmap, colorSampler, pin.wp, sun_vpm)*0.01;
	float shadowing = min(shadow, diffuse);

    float3 voxelUV = (pin.wp.xyz+30)/60;
    float3 geometryNormal = normal;
    float3 geometryB = normalize(mul(bin, (float3x3) wMat).xyz);
    float3 geometryT = normalize(mul(pin.tangent, (float3x3) wMat).xyz);

    float4 fullTraceSample = coneTrace(voxelUV, geometryNormal, middleCone.x, middleCone.y, voxelmap, colorSampler3, 0, radius) * 1.4;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal + geometryT), sideCone.x, sideCone.y, voxelmap, colorSampler3, 0, radius) * 1.0;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal - geometryT), sideCone.x, sideCone.y, voxelmap, colorSampler3, 0, radius) * 1.0;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal + geometryB), sideCone.x, sideCone.y, voxelmap, colorSampler3, 0, radius) * 1.0;
    fullTraceSample += coneTrace(voxelUV, normalize(geometryNormal - geometryB), sideCone.x, sideCone.y, voxelmap, colorSampler3, 0, radius) * 1.0;
    float3 traceColor = fullTraceSample.rgb / 5;

#ifdef USE_VCT_SKY_AMBIENT
    float sky = saturate(1 - fullTraceSample.w / 5);
    float3 skyColor = sky * float3(0.5, 0.6, 0.9);
    traceColor += skyColor * (1 - shadowing);
#endif
	
#ifdef USE_VCT_OCCLUSION
    float occlusionSample = sampleVox(voxelmap, colorSampler3, voxelUV + geometryNormal / 128, geometryNormal, 0).w;
    occlusionSample += sampleVox(voxelmap, colorSampler3, voxelUV + normalize(geometryNormal + geometryT) / 128, normalize(geometryNormal + geometryT), 0).w;
    occlusionSample += sampleVox(voxelmap, colorSampler3, voxelUV + normalize(geometryNormal - geometryT) / 128, normalize(geometryNormal - geometryT), 0).w;
    occlusionSample += sampleVox(voxelmap, colorSampler3, voxelUV + (geometryNormal + geometryB) / 128, (geometryNormal + geometryB), 0).w;
    occlusionSample += sampleVox(voxelmap, colorSampler3, voxelUV + (geometryNormal - geometryB) / 128, (geometryNormal - geometryB), 0).w;
    occlusionSample = saturate(occlusionSample / 5);
    shadowing *= 1 - occlusionSample;
#endif

#ifdef USE_VCT_SPECULAR
    float3 specVector = reflect(normalize(pin.wp.xyz - camPos), geometryNormal);
    traceColor += coneTrace(voxelUV, specVector, radius, 1, voxelmap, colorSampler3, 1, radius).rgb;
#endif

    float4 albedo = colorMap.Sample(colorSampler, pin.uv);

    float4 color1 = saturate(albedo * float4(traceColor + shadowing, 0));

#ifdef USE_VC
	color1.rgb += pin.color;
#endif

    return color1;
}
