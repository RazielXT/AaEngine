#include "PostProcessCommon.hlsl"

Texture2D accumulatedTex : register(t0);
Texture2D blurredTex : register(t1);
Texture2D<float> ageTex : register(t2);

static const float MaxAge = 32.0f;
static const float FreshBlurStrength = 1;

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float4 accumulated = accumulatedTex.Load(int3(input.Position.xy, 0));
	float4 blurred = blurredTex.Load(int3(input.Position.xy, 0));
	float age = ageTex.Load(int3(input.Position.xy, 0));

	float freshness = saturate(1.0f - age / MaxAge);
	float blendWeight = freshness * FreshBlurStrength;

	return lerp(accumulated, blurred, saturate(0.2 + blendWeight));
}
