cbuffer PerObject : register(b1) {
    float4x4 wvp : packoffset(c0);
};

cbuffer PerMaterial : register(b2) {
    float4 col : packoffset(c0);
};

struct VS_Input
{
float4 pos : POSITION;
float2 uv : TEXCOORD0;
};
struct PS_Input
{
float4 pos : SV_POSITION;
float2 uv : TEXCOORD0;
};

PS_Input VS_Main( VS_Input vertex )
{
PS_Input vsOut = ( PS_Input )0;
vsOut.pos = mul( vertex.pos, wvp);
vsOut.uv = vertex.uv*float2(1,-1);
return vsOut;
}

float4 PS_Main( PS_Input frag,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0),
			Texture2D colorMap2 : register(t1),
			SamplerState colorSampler2 : register(s1)
		     ) : SV_TARGET
{
float4 color1=colorMap.Sample( colorSampler, frag.uv );
float4 color2=colorMap2.Sample( colorSampler2, frag.uv*2 );

if(color2.g>0.6) discard;

color1 *= col;

return color1;
}

float4 PS_Main2( PS_Input frag,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0)
		     ) : SV_TARGET
{
return colorMap.Sample( colorSampler, frag.uv ).r;
}