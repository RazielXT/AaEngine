float4x4 ViewProjectionMatrix;
float3 CameraPosition;

struct VSInput
{
	float4 position : POSITION;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float3 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input) // uint vertexIdx : SV_VertexId
{
	PSInput result;

	//float3 vertexPosition = vertices[indices[vertexIdx]];
	float3 vertexPosition = input.position.xyz;
	vertexPosition.y -= 0.5;
	vertexPosition *= 1000;
	result.uv = normalize(-vertexPosition);

	vertexPosition += CameraPosition;

	result.position = mul(float4(vertexPosition, 1), ViewProjectionMatrix);

	return result;
}

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normals : SV_Target1;
	float4 camDistance : SV_Target2;
	float4 motionVectors : SV_Target3;
};

TextureCube skyboxTexture : register(t0);
SamplerState samplerState : register(s0);

PSOutput PSMain(PSInput input)
{
	PSOutput output;

	output.albedo = float4(skyboxTexture.Sample(samplerState, input.uv).rgb, 0);
	output.normals = 0;
	output.camDistance = 0;
	output.motionVectors = 0;

	return output;
}
