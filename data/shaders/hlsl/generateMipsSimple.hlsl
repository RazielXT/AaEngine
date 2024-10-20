uint TextureSize;

SamplerState LinearClampSampler : register(s0);
Texture3D<float4> inputTexture : register(t0);
RWTexture3D<float4> mipTexture : register(u0);

[numthreads(8,8,8)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
	uint mipLevel = 1;
	float3 texCoord = (float3(id) + 0.5f) / (TextureSize / pow(2, mipLevel));
    float4 val = inputTexture.SampleLevel(LinearClampSampler, texCoord, 0);
    mipTexture[id] = float4(val.rgb, 1);
}
