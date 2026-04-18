#ifndef TRIPLANAR_HLSL
#define TRIPLANAR_HLSL

#include "hlsl/common/ResourceAccess.hlsl"

float4 SampleTriplanar(uint texId, SamplerState sampler, float3 worldPos, float3 worldNormal, float scale)
{
	float3 blendWeights = abs(worldNormal.xyz);
	// Tighten up the blending zone:
	blendWeights = (blendWeights - 0.2) * 7;
	blendWeights = max(blendWeights, 0);
	blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z).xxx;

	float2 uvX = worldPos.yz * scale;
	float2 uvY = worldPos.zx * scale;
	float2 uvZ = worldPos.xy * scale;

	float4 col1 = GetTexture2D(texId).Sample(sampler, uvX);
	float4 col2 = GetTexture2D(texId).Sample(sampler, uvY);
	float4 col3 = GetTexture2D(texId).Sample(sampler, uvZ);

	return col1 * blendWeights.x + col2 * blendWeights.y + col3 * blendWeights.z;
}

float3 SampleTriplanarNormal(uint texId, SamplerState sampler, float3 worldPos, float3 worldNormal, float scale)
{
	float3 blendWeights = abs(worldNormal.xyz);
	// Tighten up the blending zone:
	blendWeights = (blendWeights - 0.2) * 7;
	blendWeights = max(blendWeights, 0);
	blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z).xxx;

	float2 uvX = worldPos.zy * scale;
	float2 uvY = worldPos.xz * scale;
	float2 uvZ = worldPos.xy * scale;

	// Tangent space normal maps
	float3 tnormalX = GetTexture2D(texId).Sample(sampler, uvX).rgb * 2 - 1;
	float3 tnormalY = GetTexture2D(texId).Sample(sampler, uvY).rgb * 2 - 1;
	float3 tnormalZ = GetTexture2D(texId).Sample(sampler, uvZ).rgb * 2 - 1;

	// Swizzle world normals into tangent space and apply UDN blend.
	// These should get normalized, but it's very a minor visual
	// difference to skip it until after the blend.
	tnormalX = float3(tnormalX.xy + worldNormal.zy, worldNormal.x);
	tnormalY = float3(tnormalY.xy + worldNormal.xz, worldNormal.y);
	tnormalZ = float3(tnormalZ.xy + worldNormal.xy, worldNormal.z);

	// Swizzle tangent normals to match world orientation and triblend
	return normalize(
		tnormalX.zyx * blendWeights.x +
		tnormalY.xzy * blendWeights.y +
		tnormalZ.xyz * blendWeights.z
	);
}

#endif
