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

float4 PS_Main( PS_Input pin,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0),
			Texture2D normalmap : register(t1),
			SamplerState colorSampler1 : register(s1),
			Texture2D shadowmap : register(t2),
			SamplerState colorSampler2 : register(s2)
		     ) : SV_TARGET
{

float3 nSunDir = normalize(sunDir);

float3 normalTex=normalmap.Sample( colorSampler1, pin.uv );
float3x3 tbn = float3x3(pin.tangent, cross(pin.normal,pin.tangent), pin.normal);
float3 normal = mul(transpose(tbn), normalTex.xyz * 2 - 1); // to object space
normal = normalize(mul(normal,wMat).xyz);

float diffuse = max(0,dot(-nSunDir, normal)).x;

float shadow = min(vsmShadow(shadowmap,colorSampler2,pin.wp,sun_vpm),diffuse)*0.75 + 0.25;

float4 color1=colorMap.Sample( colorSampler, pin.uv );
color1.rgb *= shadow;

return color1;
}