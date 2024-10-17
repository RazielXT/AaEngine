float4x4 WorldViewProjectionMatrix;
float4x4 ViewProjectionMatrix;

#ifdef INSTANCED
cbuffer Instancing : register(b2)
{
	float4x4 instanceTransform[1024];
}
#endif

struct VS_INPUT
{
    float4 position : POSITION;
#ifdef INSTANCED
	uint instanceID : SV_InstanceID;
#endif
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
};


VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;

#ifdef INSTANCED
	float4 worldPosition = mul(Input.position, instanceTransform[Input.instanceID]);
    Output.position = mul(worldPosition, ViewProjectionMatrix);
#else
	Output.position = mul(Input.position, WorldViewProjectionMatrix);
#endif

    return Output;
}
