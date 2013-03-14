cbuffer PerObject : register(b0) {
    float4x4 wvp;
    float4x4 rvp;
    float4 scissors;
    float alpha;
};


struct VS_Input
{
float4 pos : POSITION;
float4 c : COLOR;
float2 uv : TEXCOORD0;
};

struct PS_Input
{
float4 pos : SV_POSITION;
float4 c : COLOR;
float2 uv : TEXCOORD0;
};

PS_Input VS_Main( VS_Input vertex )
{
PS_Input vsOut = ( PS_Input )0;
vsOut.pos = mul( vertex.pos, wvp);
vsOut.pos.y *= -1;

vsOut.c = vertex.c;
vsOut.c.a *= alpha;
vsOut.uv = vertex.uv;
return vsOut;
}

float4 PS_Main( PS_Input frag,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0)
		     ) : SV_TARGET
{
if(frag.pos.y<scissors.b || frag.pos.y>scissors.a)
discard;

float4 color1=colorMap.Sample( colorSampler, frag.uv );
return color1*frag.c*alpha;
}

float4 PS_Main2( PS_Input frag) : SV_TARGET
{
if(frag.pos.y<scissors.b || frag.pos.y>scissors.a)
discard;

return frag.c*alpha;
}
