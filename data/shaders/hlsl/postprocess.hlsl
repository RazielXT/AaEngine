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

static const float BlurSize = 0.002;

Texture2D colorMap : register(t0);
SamplerState LinearSampler : register(s0);

float4 PSPassThrough(VS_OUTPUT input) : SV_TARGET
{
    return colorMap.Sample(LinearSampler, input.TexCoord);
}

float4 PSPassThroughIllumination(VS_OUTPUT input) : SV_TARGET
{
	float4 sample = colorMap.Sample(LinearSampler, input.TexCoord);
    return sample * sample.a * 2;
}

float4 PSPassThroughAvg(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(LinearSampler, input.TexCoord);
	float avg = length(color.rgb)/1.5; //color.r + color.g + color.b / 3;
    return float4(avg.rrr, color.a);
}

float4 PSPutBlurThrough(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(LinearSampler, input.TexCoord);
	float offset = BlurSize;
	color += colorMap.Sample(LinearSampler, input.TexCoord + float2(offset, offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + float2(-offset,offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + float2(offset,-offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + float2(-offset,-offset));

    return color / 5;
}

float4 PSBlurX(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(LinearSampler, input.TexCoord);
	float offset = BlurSize;
	float samples = 4;
	for (float i = 1; i < samples; i++)
	{
		color += colorMap.Sample(LinearSampler, input.TexCoord + float2(offset * i,0));
		color += colorMap.Sample(LinearSampler, input.TexCoord + float2(-offset * i,0));
	}

    return color / (1 + samples * 2);
}

float4 PSBlurY(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(LinearSampler, input.TexCoord);
	float offset = BlurSize;
	float samples = 4;
	for (float i = 1; i < samples; i++)
	{
		color += colorMap.Sample(LinearSampler, input.TexCoord + float2(0, offset * i));
		color += colorMap.Sample(LinearSampler, input.TexCoord + float2(0, -offset * i));
	}

    return color / (1 + samples * 2);
}

Texture2D colorMap2 : register(t1);
Texture2D godrayMap : register(t2);
Texture2D exposureMap : register(t3);
Texture2D<float> distanceMap : register(t4);

float4 PSAddThrough(VS_OUTPUT input) : SV_TARGET
{
	float4 original = colorMap.Sample(LinearSampler, input.TexCoord);
	float exposure = 1 - exposureMap.Load(int3(0, 0, 0)).r;

	float3 fogColor = float3(0.6,0.6,0.7);
	float camDistance = distanceMap.Load(int3(input.Position.xy, 0)).r;
	original.rgb = lerp(original.rgb, fogColor, min(0.85, saturate((camDistance - 1000) / 14000)));

	float3 bloom = colorMap2.Sample(LinearSampler, input.TexCoord).rgb;
	bloom *= bloom;
	bloom *= exposure * exposure;

#ifdef NO_LUMA
	original.w = 1;
#else
	float luma = dot(original.rgb, float3(0.299, 0.587, 0.114));
	original.w = luma;
#endif

	original.rgb *= 1 + exposure * exposure;
	original.rgb += bloom;
	original.rgb += godrayMap.Sample(LinearSampler, input.TexCoord).rgb * exposure;

    return original;
}

float4 PSDarken(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(LinearSampler, input.TexCoord);
	color.rgb *= lerp(colorMap2.Sample(LinearSampler, input.TexCoord).r, 1, saturate(color.a));

    return color;
}

float4 PSSkyPassThrough(VS_OUTPUT input) : SV_TARGET
{
	if (colorMap2.Sample(LinearSampler, input.TexCoord).a < 1)
		return colorMap.Sample(LinearSampler, input.TexCoord);

    return float4(0, 0, 0, 0);
}