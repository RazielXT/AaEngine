#include "PostProcessCommon.hlsl"

Texture2D colorMap : register(t0);
Texture2D bloomMap : register(t1);
Texture2D<float> exposureMap : register(t2);
Texture2D godrayMap : register(t3);

SamplerState LinearSampler : register(s0);

// Narkowicz ACES Fitted Approximation
float3 AcesApprox(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 LinearToPQ(float3 L)
{
    const float m1 = 2610.0 / 4096.0 / 4.0;
    const float m2 = 2523.0 / 4096.0 * 128.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 4096.0 * 32.0;
    const float c3 = 2392.0 / 4096.0 * 32.0;

    float3 Lm1 = pow(L, m1);
    return pow((c1 + c2 * Lm1) / (1.0 + c3 * Lm1), m2);
}

float3 ConvertToHDR10(float3 sceneLinearHDR)
{
	//float SDRWhiteLevel = 80;
    float DisplayPeak = 600;

    // 1. Tone map scene to display peak
    float3 nits = sceneLinearHDR * DisplayPeak;

    // 2. Convert Rec.709 → Rec.2020
    const float3x3 R709_to_R2020 = float3x3(
        0.6274, 0.3293, 0.0433,
        0.0691, 0.9195, 0.0114,
        0.0164, 0.0880, 0.8956
    );
	float3 color2020 = mul(R709_to_R2020, nits);

	// 3. Normalize for PQ (0..10000 nits)
	float3 L = saturate(color2020 / 10000.0);

	// 4. PQ encode
    return LinearToPQ(L);
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float3 original = colorMap.Sample(LinearSampler, input.TexCoord).rgb;

	float exposure = exposureMap.Load(int3(0, 0, 0)).r;
	//brightness = smoothstep(0.2, 0.8, exposure);
	float darkness = 1 - exposure;
	darkness = smoothstep(0.2, 1, darkness);

	float3 bloom = bloomMap.Sample(LinearSampler, input.TexCoord).rgb;
	bloom *= darkness;

	float3 finalImage = original;
	finalImage *= (1 + darkness);

	finalImage += bloom;
	finalImage += godrayMap.Sample(LinearSampler, input.TexCoord).rgb;

	//finalImage = finalImage * 0.001 + original;

	float3 toneMapped = AcesApprox(finalImage);

#ifdef CFG_HDR
	toneMapped = ConvertToHDR10(toneMapped);
#else //srgb
	toneMapped = pow(toneMapped, 1.0/2.233);
#endif

	return float4(toneMapped, 1);
}
