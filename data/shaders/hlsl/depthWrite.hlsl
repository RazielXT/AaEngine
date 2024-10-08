//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
cbuffer PerObject : register(b1)
{
    float4x4 wvpMat : packoffset(c0);
};

//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 vPosition : POSITION;
};

struct VS_OUTPUT
{
    float4 vPosition : SV_POSITION;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;
		
    Output.vPosition = mul(Input.vPosition, wvpMat);
    Output.vPosition.z += 0.001;
	
    return Output;
}


float2 PSMain(VS_OUTPUT Input) : SV_TARGET
{
    float2 rt = float2(Input.vPosition.z, Input.vPosition.z * Input.vPosition.z);
    return rt;
}