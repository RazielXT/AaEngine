float4x4 ViewProjectionMatrix;
uint TexIdDiffuse;
float Time;

cbuffer GrassPositionsBuffer : register(b1)
{
	float4 GrassBottomPositions[1024];
}

struct PSInput
{
    float4 position : SV_POSITION;
	float2 uv : TEXCOORD1;
};

/*
2,4	3

0	1,5
*/
PSInput VSMain(uint vertexIdx : SV_VertexId)
{
    PSInput output;

	uint quadID = vertexIdx / 6;
    uint vertexID = vertexIdx % 6;
	uint xOffset = vertexID % 2;
	uint yOffset = vertexID > 1 && vertexID < 5;

	float4 pos = GrassBottomPositions[quadID * 2 + xOffset];
	
	float grassHeight = 4;
	pos.y += yOffset * grassHeight;

	float3 windDirection = float3(1,0,1);
	float frequency = 1;
	pos.xyz += yOffset * windDirection * sin(Time * frequency + pos.x);

	float2 uv = float2(xOffset, 1.0f - yOffset);

    // Output position and UV
    output.position = mul(pos, ViewProjectionMatrix);
    output.uv = uv;

    return output;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState g_sampler : register(s0);

struct PSOutput
{
    float4 target0 : SV_Target0;
    float4 target1 : SV_Target1;
};

PSOutput PSMain(PSInput input)
{
	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, input.uv);

	if (albedo.a <0.5) discard;

	albedo.rgb *= float3(0.1,0.5,0.1);

	PSOutput output;
    output.target0 = albedo;
	output.target1 = albedo * albedo;

	return output;
}
