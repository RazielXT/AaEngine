float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;

#ifdef INSTANCED
StructuredBuffer<float4x4> InstancingBuffer : register(t0);
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
	float4 worldPosition = mul(Input.position, InstancingBuffer[Input.instanceID]);
    Output.position = mul(worldPosition, ViewProjectionMatrix);
#else
	Output.position = mul(mul(Input.position, WorldMatrix), ViewProjectionMatrix);
#endif

    return Output;
}
