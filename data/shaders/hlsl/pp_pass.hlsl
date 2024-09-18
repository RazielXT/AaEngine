struct PS_Input
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 PS_MainAddT(PS_Input frag,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0),
			Texture2D colorMap2 : register(t1),
			SamplerState colorSampler2 : register(s1)
		     ) : SV_TARGET
{
    float4 color1 = colorMap.Sample(colorSampler, frag.uv);
    color1.rgb += colorMap2.Sample(colorSampler2, frag.uv).rgb;

    return color1;
}

float4 PS_MainLum(PS_Input frag,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0)
		     ) : SV_TARGET
{
    float4 color = colorMap.Sample(colorSampler, frag.uv);

    return pow(color - 0.3, 3);
}