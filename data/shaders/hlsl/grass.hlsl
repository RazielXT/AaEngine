float4x4 ViewProjectionMatrix;
uint TexIdDiffuse;
float Time;

StructuredBuffer<float4> GeometryBuffer : register(t0);

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

	float4 pos = GeometryBuffer[quadID * 2 + xOffset];
	
	float grassHeight = 4;
	pos.y += yOffset * grassHeight;

	float3 windDirection = float3(1,0,1);
	float frequency = 1;
	pos.xyz += yOffset * windDirection * sin(Time * frequency + pos.x * pos.z + pos.y);

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

	float distanceFade = saturate(200 * input.position.z / input.position.w);

	if (albedo.a * distanceFade <0.5) discard;

	albedo.rgb *= float3(0.1,0.5,0.1);

	PSOutput output;
    output.target0 = albedo * distanceFade;
	output.target1 = albedo * albedo;

	return output;
}

void PSMainDepth(PSInput input)
{
	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, input.uv);

	float distanceFade = saturate(200 * input.position.z / input.position.w);

	if (albedo.a * distanceFade <0.5) discard;
}
