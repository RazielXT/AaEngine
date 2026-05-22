float4 vertices[4];
uint TextureIndex;
#ifdef TEXTURE_3D
uint SliceIndex;
#endif
uint ResId;
float RemapMin;
float RemapMax;

#include "hlsl/common/ResourceAccess.hlsl"

static const float2 uv[4] = {
    float2(1.0f, 1.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),	
    float2(0.0f, 0.0f)
};

static const uint indices[6] = {
    0, 1, 2, // First triangle
    2, 1, 3  // Second triangle
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// Manually positioned quad.
VS_OUTPUT VSQuad(uint vIndex : SV_VertexId)
{
    VS_OUTPUT vout;

    vout.uv = uv[indices[vIndex]];
    vout.position = vertices[indices[vIndex]];

    return vout;
}

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

float4 PSTexturePreview(VS_OUTPUT input) : SV_TARGET
{
#ifdef TEXTURE_3D
	#ifdef TEXTURE_UINT
		Texture3D<uint4> tex = GetTexture3Du(TextureIndex);
		uint w, h, d;
		tex.GetDimensions(w, h, d);
		uint3 coord = uint3(input.uv * float2(w, h), SliceIndex);
		float4 color = float4(tex.Load(int4(coord, 0)));
	#else
		Texture3D<float4> tex = GetTexture3D(TextureIndex);
		uint w, h, d;
		tex.GetDimensions(w, h, d);
		float slice = (SliceIndex + 0.5f) / float(d);
		float4 color = tex.Sample(PointSampler, float3(input.uv, slice));
	#endif
#else
	#ifdef TEXTURE_UINT
		Texture2D<uint4> tex = GetTexture2Du(TextureIndex);
		uint w, h;
		tex.GetDimensions(w, h);
		uint2 coord = uint2(input.uv * float2(w, h));
		float4 color = float4(tex.Load(int3(coord, 0)));
	#else
		float4 color = GetTexture2D(TextureIndex).Sample(LinearSampler, input.uv);
	#endif
#endif

	color.rgb = (color.rgb - RemapMin) / (RemapMax - RemapMin);

	return float4(color.rgb, 1);
}
