cbuffer PerFrame : register(b0)
{
    float4x4 sun_vpm : packoffset(c4);
};

cbuffer PerObject : register(b1)
{
    float4x4 wvp : packoffset(c0);
    float4x4 wMat : packoffset(c4);
};

cbuffer PerMaterial : register(b2)
{
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
    float4 wp : TEXCOORD1;
};

PS_Input VS_Main(VS_Input vertex)
{
    PS_Input vsOut = (PS_Input) 0;
    vsOut.pos = mul(vertex.pos, wvp);
    vsOut.wp = mul(vertex.pos, wMat);
    vsOut.uv = vertex.uv * float2(1, -1);
    return vsOut;
}

float4 PS_Main(PS_Input frag,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0),
			Texture2D colorMap2 : register(t1),
			SamplerState colorSampler2 : register(s1),
			SamplerState colorSampler3 : register(s2),
			Texture2D shadowmap : register(t2)
		     ) : SV_TARGET
{
    float4 color1 = colorMap.Sample(colorSampler, frag.uv);
    float4 color2 = colorMap2.Sample(colorSampler2, frag.uv * 2);

    float4 sunLookPos = mul(frag.wp, transpose(sun_vpm));
    sunLookPos.xyz /= sunLookPos.w;
    sunLookPos.xy /= float2(2, -2);
    sunLookPos.xy += 0.5;

/*float2 moments = shadowmap.Sample( colorSampler3, sunLookPos.xy).xy;
float bias = 0.0003;
if(sunLookPos.z-bias>moments.x) color1*=0.5;*/

    float2 moments = float2(0, 0);

    int steps = 3;
    half stepSize = 0.002f;

    for (int x = 0; x < steps; ++x)
        for (int y = 0; y < steps; ++y)
            moments += shadowmap.Sample(colorSampler3, float2(sunLookPos.xy + float2(x * stepSize, y * stepSize))).xy;

    moments /= steps * steps;
	
/*float4 rSamples = shadowmap.GatherRed(colorSampler3, sunLookPos.xy,int2(1,1));
moments.r = (rSamples.r+rSamples.g+rSamples.b+rSamples.a)/4;
rSamples = shadowmap.GatherGreen(colorSampler3, sunLookPos.xy,int2(1,1));
moments.g = (rSamples.r+rSamples.g+rSamples.b+rSamples.a)/4;*/

    float ourDepth = sunLookPos.z;
    float litFactor = (ourDepth <= moments.x ? 1 : 0);
// standard variance shadow mapping code
    float E_x2 = moments.y;
    float Ex_2 = moments.x * moments.x;
    float vsmEpsilon = 0.000001;
    float variance = min(max(E_x2 - Ex_2, 0.0) + vsmEpsilon, 1.0);
    float m_d = moments.x - ourDepth;
    float p = variance / (variance + m_d * m_d);

    float shadow = smoothstep(0.1, 1, max(litFactor, p));
    color1 *= 0.5 + shadow * 0.5;

    return color1;
}