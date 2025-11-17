#include "PostProcessCommon.hlsl"

Texture2D colorMap : register(t0);
Texture2D bloomMap : register(t1);
Texture2D<float> exposureMap : register(t2);
Texture2D godrayMap : register(t3);

SamplerState LinearSampler : register(s0);

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float4 original = colorMap.Sample(LinearSampler, input.TexCoord);
	float exposure = 1 - exposureMap.Load(int3(0, 0, 0)).r;

	float3 bloom = bloomMap.Sample(LinearSampler, input.TexCoord).rgb;
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
