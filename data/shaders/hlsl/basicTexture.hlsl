cbuffer PerObject : register(b1)
{
    float4x4 wvp : packoffset(c0);
};

struct VS_Input
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};
struct VS_Output
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VS_Output VS_Main(VS_Input vin)
{
	VS_Output vsOut = (VS_Output) 0;
	vsOut.pos = mul(vin.pos, wvp);
	vsOut.uv = vin.uv;
	return vsOut;
}

float4 PS_Main(PS_Input pin,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0)
		     ) : SV_TARGET
{
	return colorMap.Sample( colorSampler, pin.uv );
}
