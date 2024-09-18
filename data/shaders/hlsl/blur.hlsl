struct PS_Input
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 PS_BlurX(PS_Input frag,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0)
		     ) : SV_TARGET
{
    float4 color1 = 0;

    for (int i = -4; i <= 4; i++)
        color1 += colorMap.Sample(colorSampler, frag.uv + float2(0.007 * i, 0));

    return color1 / 9;

}

float4 PS_BlurY(PS_Input frag,
			Texture2D colorMap : register(t0),
			SamplerState colorSampler : register(s0)
		     ) : SV_TARGET
{
    float4 color1 = 0;

    for (int i = -4; i <= 4; i++)
        color1 += colorMap.Sample(colorSampler, frag.uv + float2(0, 0.007 * i));

    return color1 / 9;
}