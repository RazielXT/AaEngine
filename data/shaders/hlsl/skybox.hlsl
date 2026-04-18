#include "hlsl/common/ShaderOutputs.hlsl"

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

PSInput VSMain(VSInput input)
{
	PSInput result;

	float3 vertexPosition = input.position.xyz;
	vertexPosition.y -= 0.5;
	vertexPosition *= 10000;
	result.uv = normalize(-vertexPosition);

	vertexPosition += CameraPosition;

	result.position = mul(float4(vertexPosition, 1), ViewProjectionMatrix);

	return result;
}

TextureCube skyboxTexture : register(t0);
SamplerState samplerState : register(s0);

GBufferOutput PSMain(PSInput input)
{
	GBufferOutput output;

	output.albedo = float4(skyboxTexture.Sample(samplerState, input.uv).rgb, 0);
	output.normals = 0;
	output.motionVectors = 0;

	return output;
}
