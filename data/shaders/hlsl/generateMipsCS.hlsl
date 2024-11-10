uint SrcMipIndex;
float3 InvOutTexelSize;

SamplerState LinearClampSampler : register(s0);
Texture3D<float4> inputTexture : register(t0);
RWTexture3D<float4> mipTexture : register(u0);

[numthreads(8,8,8)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
	float3 texCoord = (float3(id) + 0.5f) * InvOutTexelSize;
    float4 val = inputTexture.SampleLevel(LinearClampSampler, texCoord, SrcMipIndex);
    mipTexture[id] = val;
}
