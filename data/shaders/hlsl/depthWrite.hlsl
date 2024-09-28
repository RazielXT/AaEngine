float4x4 WorldViewProjectionMatrix;

struct VS_INPUT
{
    float4 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
};

VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;
		
    Output.position = mul(Input.position, WorldViewProjectionMatrix);

    return Output;
}
