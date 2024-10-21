cbuffer PerObject : register(b1)
{
    float4x4 wvpMat : packoffset(c0);
    float4x4 wMat : packoffset(c4);
};

cbuffer PerMaterial : register(b2)
{
    float stepping : packoffset(c0);
    float stepping2 : packoffset(c0.y);
	float lightPower : packoffset(c0.z);
    float lerpFactor : packoffset(c0.w);
};

cbuffer SceneVoxelInfo : register(b3)
{
    float2 middleCone : packoffset(c0);
    float2 sideCone : packoffset(c0.z);
    float radius : packoffset(c1);
	float3 sceneCorner : packoffset(c2);
	float voxelSize : packoffset(c2.w);
};

struct VS_Input
{
    float4 p : POSITION;
    float2 uv : TEXCOORD0;
    float3 n : NORMAL;
    float3 tangent : TANGENT;
};

struct VS_Out
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 wp : TEXCOORD1;
    float3 n : TEXCOORD2;
    float3 tangent : TANGENT;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 wp : TEXCOORD1;
    float3 n : TEXCOORD2;
    float3 tangent : TANGENT;
};

VS_Out VS_Main(VS_Input vin)
{
    VS_Out vsOut = (VS_Out) 0;

    vsOut.pos = mul(vin.p, wvpMat);
    vsOut.uv = vin.uv;
    vsOut.wp = mul(vin.p, wMat).xyz;
    vsOut.n = mul(vin.n, (float3x3)wMat).xyz;
    vsOut.tangent = mul(vin.tangent, (float3x3)wMat).xyz;

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
	
	/*sampleX = saturate(sampleX);
	sampleY = saturate(sampleY);
	sampleZ = saturate(sampleZ);*/
	
    float3 wts = abs(dir);
 
    float invSampleMag = 1.0 / (wts.x + wts.y + wts.z + 0.0001);
    wts *= invSampleMag;
 
    float4 filtered;
    filtered.xyz = (sampleX.xyz * wts.x + sampleY.xyz * wts.y + sampleZ.xyz * wts.z);
    filtered.w = (sampleX.w * wts.x + sampleY.w * wts.y + sampleZ.w * wts.z);
 
    return filtered;
}
 
float4 coneTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D cmap, Texture3D nmap, SamplerState sampl, float closeZero, float radius)
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
        float4 sampleVal1 = sampleVox(cmap, sampl, samplePos, -d, sampleLOD);
		//float4 sampleVal2 = sampleVox(nmap,sampl, samplePos, -d, sampleLOD);
		//sampleVal.rgb *= sampleVal.w;
		
        float4 sampleVal = sampleVal1; //lerp(sampleVal1,sampleVal2,(d.x+1)/2);	
		
        float lodChange = max(0, sampleLOD * sampleLOD / closeZero);
        float insideVal = sampleVal.w;
        sampleVal.rgb *= lodChange;

		//float discIncrease=max(1,1 - closeZero + dist/maxDist);
		/*float facingOut = max(0,dotted);//*distF;
		if(dist>0.05)
		distChanger = max(0,distChanger-facingOut);*/
		//if(dist<=startDist*3) 
		//dotted=0;
		
		//distChanger = saturate(distChanger-saturate(dotted/5));
        lodChange = max(0, sampleLOD * closeZero);
        sampleVal.w = saturate(lodChange * insideVal);
		//if(insideVal>0)
		//sampleVal/=insideVal;
		
        float sampleWt = (1.0 - accum.w);
        accum += sampleVal * sampleWt;
		
		/*float facingIn = max(0,-dotted);//*distF;
		if(dist>0.05)
		distChanger = max(0,distChanger-facingIn);*/
		
		//distChanger=saturate(distChanger-abs(dotted/4));
		
        dist += sampleDiam;
    }
	
    accum.xyz *= accum.w;
 
	//accum.w = occlusion;
    return accum;
}


void PS_Main(PS_Input pin,
			Texture2D colorMap : register(t0),
			Texture3D bouncesVoxel : register(t1),
			Texture3D shadowVoxel : register(t2),
			SamplerState colorSampler : register(s0),
			SamplerState bouncesSampler : register(s1),
			RWTexture3D<float4> voxelMap : register(u0)
		     )
{
    float3 posUV = (pin.wp.xyz - sceneCorner) * voxelSize;
    float3 voxelUV = (pin.wp.xyz + 30) / 60;
    float3 geometryNormal = pin.n.xyz; //normalize(mul(pin.normal,(float3x3)wMat).xyz);
    float3 bin = cross(pin.n, pin.tangent);
    float3 geometryB = normalize(mul(bin, (float3x3) wMat).xyz);
    float3 geometryT = normalize(mul(pin.tangent, (float3x3) wMat).xyz);
    float4 fullSample = coneTrace(voxelUV, geometryNormal, middleCone.x, middleCone.y, bouncesVoxel, bouncesVoxel, bouncesSampler, 0, radius) * 1.5;
    fullSample += coneTrace(voxelUV, normalize(geometryNormal + geometryT), sideCone.x, sideCone.y, bouncesVoxel, bouncesVoxel, bouncesSampler, 0, radius) * 1.0;
    fullSample += coneTrace(voxelUV, normalize(geometryNormal - geometryT), sideCone.x, sideCone.y, bouncesVoxel, bouncesVoxel, bouncesSampler, 0, radius) * 1.0;
    fullSample += coneTrace(voxelUV, normalize(geometryNormal + geometryB), sideCone.x, sideCone.y, bouncesVoxel, bouncesVoxel, bouncesSampler, 0, radius) * 1.0;
    fullSample += coneTrace(voxelUV, normalize(geometryNormal - geometryB), sideCone.x, sideCone.y, bouncesVoxel, bouncesVoxel, bouncesSampler, 0, radius) * 1.0;

    float4 color = colorMap.Sample(colorSampler, pin.uv);
    float3 outCol = pow(color.rgb, 1);
    float shadow = shadowVoxel.Load(float4(posUV, 0)).r;

    float3 voxCol = outCol;
    //color.rgb = ContrastSaturationBrightness(color.rgb,1.2,1.1,1);

    //voxCol.rgb = color * fullSample.rgb * stepping * radius + voxCol * stepping2; //shadow*bouncesVoxel.Load(float4(posUV,0)).g;

    voxCol.rgb = color.rgb * fullSample.rgb * stepping + voxCol * stepping2;

    float3 prev = bouncesVoxel.Load(float4(posUV, 0)).rgb;
    voxCol.rgb = lerp(prev, voxCol.rgb, lerpFactor);

    voxelMap[posUV] = float4(voxCol, 1);
}

void PS_Main_Light(PS_Input pin,
			RWTexture3D<float4> voxelMap : register(u0)
		     )
{
    float3 posUV = (pin.wp.xyz - sceneCorner) * voxelSize;

    voxelMap[posUV] = float4(1, 1, 1, 0) * lightPower;
}
