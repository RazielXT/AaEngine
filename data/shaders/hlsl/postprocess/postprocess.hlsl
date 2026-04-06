#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

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

Texture2D colorMap : register(t0);
Texture2D colorMap2 : register(t1);
SamplerState LinearSampler : register(s0);

float4 PSPassThrough(VS_OUTPUT input) : SV_TARGET
{
    return colorMap.Sample(LinearSampler, input.TexCoord);
}

float4 PSPassDownsample2x(VS_OUTPUT input) : SV_TARGET
{
	float offset = 0.25;

	float4 color = 0;
	color = colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(offset, offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(-offset,offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(offset,-offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(-offset,-offset));

    return color / 4;
}

float4 PSPassThroughAvg(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Sample(LinearSampler, input.TexCoord);
	float avg = length(color.rgb)/1.5; //dot(color.rgb, float3( 0.2126, 0.7152, 0.0722 ));//color.r + color.g + color.b / 3;
    return float4(avg.rrr, color.a);
}

static const float BlurSize = 1;

float4 Gaussian3x3(Texture2D InputTex, float2 uv, float2 t)
{
    float4 c00 = InputTex.Sample(LinearSampler, uv + float2(-t.x, -t.y));
    float4 c10 = InputTex.Sample(LinearSampler, uv + float2( 0.0f, -t.y));
    float4 c20 = InputTex.Sample(LinearSampler, uv + float2( t.x, -t.y));

    float4 c01 = InputTex.Sample(LinearSampler, uv + float2(-t.x,  0.0f));
    float4 c11 = InputTex.Sample(LinearSampler, uv);
    float4 c21 = InputTex.Sample(LinearSampler, uv + float2( t.x,  0.0f));

    float4 c02 = InputTex.Sample(LinearSampler, uv + float2(-t.x,  t.y));
    float4 c12 = InputTex.Sample(LinearSampler, uv + float2( 0.0f,  t.y));
    float4 c22 = InputTex.Sample(LinearSampler, uv + float2( t.x,  t.y));

    float4 result =
          (c00 + c20 + c02 + c22) * 1.0f   // corners
        + (c10 + c01 + c21 + c12) * 2.0f   // edges
        +  c11 * 4.0f;                     // center

    return result * (1.0f / 16.0f);
}

float4 PSPutBlurThrough(VS_OUTPUT input) : SV_TARGET
{
	/*float offset = BlurSize;

	float4 color = 0;//colorMap.Sample(LinearSampler, input.TexCoord);
	color = colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(offset, offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(-offset,offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(offset,-offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(-offset,-offset));

    return color / 4;*/

	return Gaussian3x3(colorMap, input.TexCoord, ViewportSizeInverse);
}

float4 PSBloomUpscale(VS_OUTPUT input) : SV_TARGET
{
	/*float offset = BlurSize;

	float4 color = 0;// = colorMap.Sample(LinearSampler, input.TexCoord);
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(offset, offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(-offset,offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(offset,-offset));
	color += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(-offset,-offset));

	float4 color2 = 0;// = colorMap2.Sample(LinearSampler, input.TexCoord);
	color2 += colorMap2.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(offset, offset));
	color2 += colorMap2.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(-offset,offset));
	color2 += colorMap2.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(offset,-offset));
	color2 += colorMap2.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * float2(-offset,-offset));

    return max(color, color2) / 4;*/

	float4 color = Gaussian3x3(colorMap, input.TexCoord, ViewportSizeInverse * 2);
	float4 color2 = Gaussian3x3(colorMap2, input.TexCoord, ViewportSizeInverse);
	return max(color, color2);
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

float4 PSSkyPassThrough(VS_OUTPUT input) : SV_TARGET
{
	if (colorMap2.Sample(LinearSampler, input.TexCoord).r > 0)
		return float4(0, 0, 0, 0);

	float4 color = colorMap.Sample(LinearSampler, input.TexCoord);

	color = pow(color, 3);

	return color;
}