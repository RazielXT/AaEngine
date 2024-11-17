struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// Vertex shader: self-created quad.
VS_OUTPUT VSQuad(uint vI : SV_VertexId)
{
    VS_OUTPUT vout;

    // We use the 'big triangle' optimization so you only Draw 3 verticies instead of 4.
    float2 texcoord = float2((vI << 1) & 2, vI & 2);
    vout.TexCoord = texcoord;

    vout.Position = float4(texcoord.x * 2 - 1, -texcoord.y * 2 + 1, 0, 1);
    return vout;
}

static const float BlurSize = 0.005;

Texture2D colorMap : register(t0);
SamplerState colorSampler : register(s0);

float4 PSPassThrough(VS_OUTPUT input) : SV_TARGET
{
    return colorMap.Sample(colorSampler, input.TexCoord);
}

float4 PSPutBlurThrough(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(colorSampler, input.TexCoord);
	float offset = BlurSize;
	color += colorMap.Sample(colorSampler, input.TexCoord + float2(offset, offset));
	color += colorMap.Sample(colorSampler, input.TexCoord + float2(-offset,offset));
	color += colorMap.Sample(colorSampler, input.TexCoord + float2(offset,-offset));
	color += colorMap.Sample(colorSampler, input.TexCoord + float2(-offset,-offset));

    return color / 5;
}

float4 PSBlurX(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(colorSampler, input.TexCoord);
	float offset = BlurSize;
	float samples = 4;
	for (float i = 1; i < samples; i++)
	{
		color += colorMap.Sample(colorSampler, input.TexCoord + float2(offset * i,0));
		color += colorMap.Sample(colorSampler, input.TexCoord + float2(-offset * i,0));
	}

    return color / (1 + samples * 2);
}

float4 PSBlurY(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(colorSampler, input.TexCoord);
	float offset = BlurSize;
	float samples = 4;
	for (float i = 1; i < samples; i++)
	{
		color += colorMap.Sample(colorSampler, input.TexCoord + float2(0, offset * i));
		color += colorMap.Sample(colorSampler, input.TexCoord + float2(0, -offset * i));
	}

    return color / (1 + samples * 2);
}

Texture2D colorMap2 : register(t1);

float4 PSAddThrough(VS_OUTPUT input) : SV_TARGET
{
	float4 original = colorMap.Sample(colorSampler, input.TexCoord);
	
	float3 bloom = colorMap2.Sample(colorSampler, input.TexCoord).rgb;
	float offset = BlurSize;
	bloom += colorMap2.Sample(colorSampler, input.TexCoord + float2(offset, offset)).rgb;
	bloom += colorMap2.Sample(colorSampler, input.TexCoord + float2(-offset,offset)).rgb;
	bloom += colorMap2.Sample(colorSampler, input.TexCoord + float2(offset,-offset)).rgb;
	bloom += colorMap2.Sample(colorSampler, input.TexCoord + float2(-offset,-offset)).rgb;
	bloom /= 5;
	bloom *= bloom;

	float luma = dot(original.rgb, float3(0.299, 0.587, 0.114));
	original.w = luma;

	original.rgb += bloom;
    return original;
}

float4 PSDarken(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(colorSampler, input.TexCoord);
	color.rbg *= 0.5 + 0.5 * colorMap2.Sample(colorSampler, input.TexCoord).r;

    return color;
}
