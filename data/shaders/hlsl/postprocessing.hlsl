struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

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
