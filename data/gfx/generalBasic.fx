cbuffer PerFrame : register(b0) {
    float3 camPos : packoffset(c4);
	float3 ambient : packoffset(c5);
    float3 sunDir : packoffset(c6);
    float3 sunColor : packoffset(c7);
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
	float3 wp : TEXCOORD2;
};

struct PS_Input
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 wp : TEXCOORD2;
};

VS_Out VS_Main( VS_Input vin )
{

VS_Out vsOut = ( VS_Out )0;

vsOut.pos = mul( vin.p, wvpMat);
vsOut.uv = vin.uv*float2(1,-1);
vsOut.normal = mul(vin.n,wMat).xyz;
vsOut.tangent = vin.t;
vsOut.wp = mul(vin.p,wMat).xyz;

return vsOut;

}

float4 PS_Main( PS_Input pin,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0),
			Texture2D colorMap2 : register(t1),
			SamplerState colorSampler2 : register(s1),
			RWTexture3D<float4> scene : register(u1)
		     ) : SV_TARGET
{

float3 nSunDir = normalize(sunDir);

float3 normal = normalize(pin.normal);
//normal = mul(float4(normal,1),wMat).xyz;

float3 diffuse = max(0,dot(nSunDir, normal));

float4 color1=colorMap.Sample( colorSampler, pin.uv );
float4 color2=colorMap2.Sample( colorSampler2, pin.uv*2 );

//if(color2.g>0.6) discard;

float3 posUV = (pin.wp - worldPos)*8 + float3(16,16,16);

color1.rgb *= diffuse;
scene[posUV] = color1;

return color1;
}