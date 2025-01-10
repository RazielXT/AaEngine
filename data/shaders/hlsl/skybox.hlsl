float4x4 ViewProjectionMatrix;
float3 CameraPosition;

/*
static const float3 vertices[] = {
    // Define vertices for a cube
    // Example for one face
    { { -1.0f,  1.0f, -1.0f } },
    { {  1.0f,  1.0f, -1.0f } },
    { { -1.0f, -1.0f, -1.0f } },
    { {  1.0f, -1.0f, -1.0f } }
};

static const uint indices[] = {
    // Define indices for the cube
    0, 1, 2, 1, 3, 2,
    // Add indices for other faces
};
*/

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
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

TextureCube skyboxTexture : register(t0);
SamplerState samplerState : register(s0);

PSOutput PSMain(PSInput input)
{
	PSOutput output;
	
	float3 texColor = skyboxTexture.Sample(samplerState, input.uv).rgb;
	float brightness = (texColor.r + texColor.g + texColor.b) / 2;

	output.color = float4(texColor,brightness);
	output.normals = 0;
	output.camDistance = 0;
	output.motionVectors = 0;

	return output;
}
