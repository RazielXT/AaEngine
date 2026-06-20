#include "hlsl/postprocess/PostProcessCommon.hlsl"

float4x4 ReprojectionMatrix;
uint2 ViewportSize;
float DeltaTime;

Texture2D currentMap : register(t0);
Texture2D accMap : register(t1);
Texture2D<float> waterDepthMap : register(t2);

SamplerState LinearSampler : register(s0);

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	float depth = waterDepthMap.Load(int3(input.Position.xy,0));

	float4 clipPos;
	clipPos.x = input.TexCoord.x * 2 - 1;
	clipPos.y = 1 - input.TexCoord.y * 2;
	clipPos.z = depth;
	clipPos.w = 1;

	float4 prevClipPos = mul(clipPos, ReprojectionMatrix);

	if (prevClipPos.w <= 0)
		return 0;

	prevClipPos.xyz /= prevClipPos.w;
	float2 prevUV;
	prevUV.x = 0.5 + prevClipPos.x * 0.5;
	prevUV.y = 0.5 - prevClipPos.y * 0.5;

	float4 current = currentMap.Sample(LinearSampler, input.TexCoord);
	float4 accumulated = accMap.Sample(LinearSampler, prevUV);

	return lerp(current, accumulated, saturate(0.5f - DeltaTime * 5));
}