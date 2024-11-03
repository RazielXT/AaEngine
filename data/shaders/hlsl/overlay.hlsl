float4 vertices[4];
uint TextureIndex;

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

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}
SamplerState g_sampler : register(s0);

float4 PSTexturePreview(VS_OUTPUT input) : SV_TARGET
{
    return GetTexture(TextureIndex).Sample(g_sampler, input.uv);
}
