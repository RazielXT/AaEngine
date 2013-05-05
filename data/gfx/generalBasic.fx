cbuffer PerFrame : register(b0) {
	float4x4 sun_vpm : packoffset(c4);
    float3 camPos : packoffset(c8);
	float3 ambient : packoffset(c9);
    float3 sunDir : packoffset(c10);
    float3 sunColor : packoffset(c11);
};

cbuffer PerObject : register(b1) {
    float4x4 wvpMat : packoffset(c0);
    float4x4 wMat : packoffset(c4);
	float3 worldPos : packoffset(c8);
};

struct VS_Input
{
	float4 p    : POSITION;
	float3 n    : NORMAL;
	float3 t    : TANGENT;
	float2 uv   : TEXCOORD0;
};

struct VS_Out
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float4 wp : TEXCOORD1;
};

struct PS_Input
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float4 wp : TEXCOORD1;
};

VS_Out VS_Main( VS_Input vin )
{

VS_Out vsOut = ( VS_Out )0;

vsOut.pos = mul( vin.p, wvpMat);
vsOut.uv = vin.uv*float2(1,-1);
vsOut.normal = vin.n;
vsOut.tangent = vin.t;
vsOut.wp = mul(vin.p,wMat);

return vsOut;

}


float vsmShadow(Texture2D shadowmap, SamplerState colorSampler ,float4 wp, float4x4 sun_vpm)
{
	float4 sunLookPos = mul( wp, transpose(sun_vpm));
	sunLookPos.xyz/=sunLookPos.w;
	sunLookPos.xy/=float2(2,-2);
	sunLookPos.xy+=0.5;

	float2 moments = float2(0, 0);

	int steps = 3;
	float stepSize = 0.002f;

	for (int x = 0; x < steps; ++x)
	for (int y = 0; y < steps; ++y)
		moments += shadowmap.Sample( colorSampler, float2(sunLookPos.xy + float2(x * stepSize, y * stepSize))).xy;
		
	moments/=steps*steps;

	float ourDepth = sunLookPos.z;
	float litFactor = (ourDepth <= moments.x ? 1 : 0);
	// standard variance shadow mapping code
	float E_x2 = moments.y;
	float Ex_2 = moments.x * moments.x;
	float vsmEpsilon = 0.000005;
	float variance = min(max(E_x2 - Ex_2, 0.0) + vsmEpsilon, 1.0);
	float m_d = moments.x - ourDepth;
	float p = variance / (variance + m_d * m_d);

	return smoothstep(0.5,1, max(litFactor, p));
}


float4 sampleVox(Texture3D cmap,SamplerState sampl, float3 pos, float3 dir, float lod)
{
	float voxDim = 128.0f;
	float sampOff = 1;

	float4 sample0 = cmap.SampleLevel(sampl, pos, lod);
	float4 sampleX = dir.x < 0.0 ? cmap.SampleLevel( sampl, pos-float3(sampOff,0,0)/voxDim, lod) : cmap.SampleLevel( sampl, pos+float3(sampOff,0,0)/voxDim, lod);
	float4 sampleY = dir.y < 0.0 ? cmap.SampleLevel( sampl, pos-float3(0,sampOff,0)/voxDim, lod) : cmap.SampleLevel( sampl, pos+float3(0,sampOff,0)/voxDim, lod);
	float4 sampleZ = dir.z < 0.0 ? cmap.SampleLevel( sampl, pos-float3(0,0,sampOff)/voxDim, lod) : cmap.SampleLevel( sampl, pos+float3(0,0,sampOff)/voxDim, lod);
	
	sampleX = saturate(sampleX);
	sampleY = saturate(sampleY);
	sampleZ = saturate(sampleZ);
	
	float3 wts = abs(dir);
 
	float invSampleMag = 1.0/(wts.x + wts.y + wts.z + 0.0001);
	wts *= invSampleMag;
 
	float4 filtered;
	filtered.xyz = (sampleX.xyz*wts.x + sampleY.xyz*wts.y + sampleZ.xyz*wts.z);
	filtered.w = (sampleX.w*wts.x + sampleY.w*wts.y + sampleZ.w*wts.z);
 
	return filtered;
}
 
float4 coneTrace(float3 o, float3 d, float coneRatio, float maxDist, Texture3D cmap, Texture3D nmap, SamplerState sampl,float closeZero)
{
	float voxDim = 128.0f;
	float3 samplePos = o;
	float4 accum = float4(0,0,0,0);
	float minDiam = 1.0/voxDim;
	float startDist = minDiam*2;
	float dist = startDist;
	
	closeZero*=2;
	closeZero+=1;
	
	while(dist <= maxDist && accum.w < 1.0)
	{		
		float sampleDiam = max(minDiam, coneRatio*dist);
		float sampleLOD = min(3,log2(sampleDiam*voxDim));
		float3 samplePos = o + d*dist;
		float4 sampleVal1 = sampleVox(cmap,sampl, samplePos, -d, sampleLOD);
		float4 sampleVal2 = sampleVox(nmap,sampl, samplePos, -d, sampleLOD);
		//sampleVal.rgb *= sampleVal.w;
		
		float4 sampleVal = sampleVal1;//lerp(sampleVal1,sampleVal2,(d.x+1)/2);	
		
		float lodChange = max(0,2*sampleLOD*sampleLOD/closeZero);
		float insideVal = sampleVal.w;
		sampleVal.rgb *= lodChange;

		//float discIncrease=max(1,1 - closeZero + dist/maxDist);
		/*float facingOut = max(0,dotted);//*distF;
		if(dist>0.05)
		distChanger = max(0,distChanger-facingOut);*/
		//if(dist<=startDist*3) 
		//dotted=0;
		
		//distChanger = saturate(distChanger-saturate(dotted/5));
		lodChange = max(0,sampleLOD*closeZero);
		sampleVal.w = saturate(lodChange*insideVal);
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


float4 PS_Main( PS_Input pin,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0),
			Texture2D normalmap : register(t1),
			SamplerState colorSampler1 : register(s1),
			Texture2D shadowmap : register(t2),
			SamplerState colorSampler2 : register(s2),
			Texture3D voxelmap : register(t3),
			SamplerState colorSampler3 : register(s3),
			Texture3D voxelSmap : register(t4),
			SamplerState colorSampler4 : register(s4),
			Texture3D voxelNmap : register(t5),
			SamplerState colorSampler5 : register(s5)
		     ) : SV_TARGET
{

float3 nSunDir = normalize(sunDir);

float3 normalTex=normalmap.Sample( colorSampler1, pin.uv ).rgb;
normalTex=(normalTex+float3(0.5f,0.5f,1.0f)*3)/4;
float3 bin = cross(pin.normal,pin.tangent);
float3x3 tbn = float3x3(pin.tangent, bin , pin.normal);
float3 normal = mul(transpose(tbn), normalTex.xyz * 2 - 1); // to object space
normal = normalize(mul(normal,(float3x3)wMat).xyz);

float diffuse = max(0,dot(-nSunDir, normal)).x;

float shadow = vsmShadow(shadowmap,colorSampler,pin.wp,sun_vpm);

float3 voxelUV = (pin.wp.xyz+30)/60;
float3 geometryNormal = normal;//normalize(mul(pin.normal,(float3x3)wMat).xyz);
float3 geometryB = normalize(mul(bin,(float3x3)wMat).xyz);
float3 geometryT = normalize(mul(pin.tangent,(float3x3)wMat).xyz);

float4 fullSample = coneTrace(voxelUV,geometryNormal,0.7,0.7,voxelmap,voxelNmap,colorSampler3,0)*1.4;
fullSample += coneTrace(voxelUV,normalize(geometryNormal+geometryT),0.8,0.58,voxelmap,voxelNmap,colorSampler3,0)*1.0;
fullSample += coneTrace(voxelUV,normalize(geometryNormal-geometryT),0.8,0.58,voxelmap,voxelNmap,colorSampler3,0)*1.0;
fullSample += coneTrace(voxelUV,normalize(geometryNormal+geometryB),0.8,0.58,voxelmap,voxelNmap,colorSampler3,0)*1.0;
fullSample += coneTrace(voxelUV,normalize(geometryNormal-geometryB),0.8,0.58,voxelmap,voxelNmap,colorSampler3,0)*1.0;
float3 col = fullSample.rgb/2;
//col=saturate(col);

float dirCol = sampleVox(voxelNmap,colorSampler3, voxelUV+geometryNormal/128, geometryNormal, 0).w;
dirCol += sampleVox(voxelNmap,colorSampler3, voxelUV+normalize(geometryNormal+geometryT)/128, normalize(geometryNormal+geometryT), 0).w;
dirCol += sampleVox(voxelNmap,colorSampler3, voxelUV+normalize(geometryNormal-geometryT)/128, normalize(geometryNormal-geometryT), 0).w;
dirCol += sampleVox(voxelNmap,colorSampler3, voxelUV+(geometryNormal+geometryB)/128, (geometryNormal+geometryB), 0).w;
dirCol += sampleVox(voxelNmap,colorSampler3, voxelUV+(geometryNormal-geometryB)/128, (geometryNormal-geometryB), 0).w;
dirCol=saturate(dirCol/5);

float3 specVector = reflect(normalize(pin.wp.xyz-camPos),geometryNormal);
//col += coneTrace(voxelUV,specVector,0.15,1,voxelmap,voxelNmap,colorSampler3,1).rgb;

//float dirCol = voxelNmap.SampleLevel( colorSampler3, voxelUV+geometryNormal/128,0 ).w+voxelNmap.SampleLevel( colorSampler3, voxelUV+geometryNormal/64,1 ).w;

float occlusion = (1-pow(dirCol,1))*0.5+0.5;
float shadowing = min(shadow,diffuse);

float4 color1= saturate(colorMap.Sample( colorSampler, pin.uv )*(float4(occlusion*col,0) + shadowing*occlusion));
//float4 color1=voxelmap.Sample( colorSampler3, voxelUV );//colorMap.Sample( colorSampler, pin.uv );//
//color1.rgb *= color1.w;
//float shad=voxelSmap.Sample( colorSampler4, voxelUV ).r;
//color1.rgb *= occlusion;
//color1.rgb *= shadow;

return color1;
}