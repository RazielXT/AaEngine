//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
	float4 vPosition	: POSITION;
	float2 vUV	: TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 vPosition	: SV_POSITION;
	float2 vUV	: TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VSMain( VS_INPUT Input )
{
	VS_OUTPUT Output;
		
	Output.vPosition = Input.vPosition;
	Output.vUV = Input.vUV;

	return Output;
}