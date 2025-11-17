#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;

Texture2D gColorHalf     : register(t0); // half-resolution color
Texture2D gDepthFull     : register(t1); // full-resolution depth
SamplerState LinearSampler : register(s0);

float4 PS_UpsampleDepthAware(VS_OUTPUT input) : SV_Target
{
    // Depth at full-res pixel
    float depthCenter = gDepthFull.Sample(LinearSampler, input.TexCoord).r;

    // Map to half-res UV via pixel-space conversion (robust to non-exact ratios)
    float2 pFull  = input.TexCoord / ViewportSizeInverse;
    float2 pHalf  = pFull * 0.5;                 // half scale; change if ratio differs
	float2 HalfResInvSize = ViewportSizeInverse * 0.5;
    float2 uvHalf = pHalf * HalfResInvSize;

    // Neighborhood in half-res around uvHalf
    // 2x2 taps (bilinear-aligned) with depth-guided weights
    float2 H = HalfResInvSize;

    float4 c00 = gColorHalf.Sample(LinearSampler, uvHalf + float2(-H.x, -H.y));
    float4 c10 = gColorHalf.Sample(LinearSampler, uvHalf + float2(+H.x, -H.y));
    float4 c01 = gColorHalf.Sample(LinearSampler, uvHalf + float2(-H.x, +H.y));
    float4 c11 = gColorHalf.Sample(LinearSampler, uvHalf + float2(+H.x, +H.y));

    // Reproject half-res sample positions back to full-res to compare depths (approx)
    // Using nearest full-res UVs for simplicity
    float2 uvFull00 = (pHalf + float2(-0.5, -0.5)) * 2.0 * ViewportSizeInverse;
    float2 uvFull10 = (pHalf + float2(+0.5, -0.5)) * 2.0 * ViewportSizeInverse;
    float2 uvFull01 = (pHalf + float2(-0.5, +0.5)) * 2.0 * ViewportSizeInverse;
    float2 uvFull11 = (pHalf + float2(+0.5, +0.5)) * 2.0 * ViewportSizeInverse;

    float d00 = gDepthFull.Sample(LinearSampler, uvFull00).r;
    float d10 = gDepthFull.Sample(LinearSampler, uvFull10).r;
    float d01 = gDepthFull.Sample(LinearSampler, uvFull01).r;
    float d11 = gDepthFull.Sample(LinearSampler, uvFull11).r;

    // Joint bilateral weights based on depth similarity
    // Tunables
    const float sigmaDepth = 0.01; // adjust to your depth range/encoding
    const float eps = 1e-6;

    float w00 = exp(-abs(d00 - depthCenter) / sigmaDepth);
    float w10 = exp(-abs(d10 - depthCenter) / sigmaDepth);
    float w01 = exp(-abs(d01 - depthCenter) / sigmaDepth);
    float w11 = exp(-abs(d11 - depthCenter) / sigmaDepth);

    float wSum = max(w00 + w10 + w01 + w11, eps);

    float4 color = (c00 * w00 + c10 * w10 + c01 * w01 + c11 * w11) / wSum;

    return color;
}