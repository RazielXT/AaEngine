#include "hlsl/common/NormalDecoding.hlsl"

#if defined(GRID_SAMPLE_BICUBIC) || defined(GRID_SAMPLE_FAST_BICUBIC)
#include "hlsl/common/BicubicSampling.hlsl"
#endif

float2 RemapGridTextureUV(float2 gridUV)
{
#ifdef GRID_NO_PADDING
	return gridUV;
#else
	// We want to skip 32 pixels on the left, and use the next 960 pixels
	float margin = 32.0f / 1024.0f; // 0.03125
	float content = 960.0f / 1024.0f; // 0.9375

	return margin + (gridUV * content);
#endif
}

float2 SampleGridNormalBlurred(Texture2D<float2> tex, SamplerState samp, float2 uv, float invResHalf)
{
	float2 s1 = tex.Sample(samp, uv + float2( invResHalf, 0));
	float2 s2 = tex.Sample(samp, uv + float2(-invResHalf, 0));
	float2 s3 = tex.Sample(samp, uv + float2(0, invResHalf));
	float2 s4 = tex.Sample(samp, uv + float2(0, -invResHalf));

	return (s1 + s2 + s3 + s4) * 0.25;
}

float3 ReadGridNormal(Texture2D<float2> texture, SamplerState sampler, float2 uv)
{
	float2 normalXZ = SampleGridNormalBlurred(texture, sampler, RemapGridTextureUV(uv), 0.5f/1024);//NormalMap.Sample(LinearSampler, uv);

	return DecodeNormalSNORM(normalXZ);
}

struct GridTBN
{
	float3 N;
	float3 T;
	float3 B;
};

float3 ReadGridNormalRaw(Texture2D<float2> texture, SamplerState sampler, float2 uv)
{
	float invResHalf = 0.5f / 1024.0f;
	float2 texcoord = RemapGridTextureUV(uv);
	return DecodeNormalSNORM(texture.SampleLevel(sampler, texcoord, 0));
}

GridTBN ReadGridTBN(Texture2D<float2> texture, SamplerState sampler, float2 uv)
{
	float invResHalf = 0.5f / 1024.0f;
	float2 texcoord = RemapGridTextureUV(uv);

	// 1. Sample neighboring normals to get the average surface normal
	float2 s1 = texture.Sample(sampler, texcoord + float2( invResHalf, 0));
	float2 s2 = texture.Sample(sampler, texcoord + float2(-invResHalf, 0));
	float2 s3 = texture.Sample(sampler, texcoord + float2(0,  invResHalf));
	float2 s4 = texture.Sample(sampler, texcoord + float2(0, -invResHalf));

	float2 normalXZ = (s1 + s2 + s3 + s4) * 0.25f;
	float3 N = DecodeNormalSNORM(normalXZ);

	// 2. Define a stable world-space tangent reference.
	// For most terrains, the Tangent (U direction) is simply +X (1, 0, 0).
	float3 worldTangent = float3(1, 0, 0);

	// 3. Orthonormalize the tangent to the normal (Gram-Schmidt)
	// This ensures T is perpendicular to N even on flat surfaces.
	float3 T = normalize(worldTangent - N * dot(worldTangent, N));

	// 4. Calculate Bitangent via cross product
	// This completes the basis and handles the "slope" automatically.
	float3 B = cross(N, T);

	GridTBN tbn;
	tbn.N = N;
	tbn.T = T;
	tbn.B = B;

	return tbn;
}

GridTBN ReadGridTBN_(Texture2D<float2> texture, SamplerState sampler, float2 uv)
{
	float invResHalf = 0.5f/1024;
	float2 texcoord = RemapGridTextureUV(uv);

	float2 s1 = texture.Sample(sampler, texcoord + float2( invResHalf, 0));
	float2 s2 = texture.Sample(sampler, texcoord + float2(-invResHalf, 0));
	float2 s3 = texture.Sample(sampler, texcoord + float2(0, invResHalf));
	float2 s4 = texture.Sample(sampler, texcoord + float2(0, -invResHalf));

	float2 normalXZ = (s1 + s2 + s3 + s4) * 0.25;
	float3 N = DecodeNormalSNORM(normalXZ);

	float3 dNdx = normalize(DecodeNormalSNORM((s1 - s2) * 0.5));
	float3 dNdy = normalize(DecodeNormalSNORM((s3 - s4) * 0.5));
	// Build tangent
	float3 T = dNdx - N * dot(N, dNdx);
	T = normalize(T);

	// Build bitangent
	float3 B = normalize(cross(N, T));

	GridTBN tbn;
	tbn.N = N;
	tbn.T = T;
	tbn.B = B;
	
	return tbn;
}
