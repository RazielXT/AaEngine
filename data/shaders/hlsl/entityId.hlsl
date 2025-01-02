float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;

#ifdef INSTANCED
uint ResIdIdsBuffer;
StructuredBuffer<float4x4> InstancingBuffer : register(t0);
#else
uint EntityId;
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
#ifdef INSTANCED
	uint entityId : TEXCOORD0;
#endif
};

VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;

#ifdef INSTANCED
	float4 worldPosition = mul(Input.position, InstancingBuffer[Input.instanceID]);
    Output.position = mul(worldPosition, ViewProjectionMatrix);
	
	StructuredBuffer<uint> idsBuffer = ResourceDescriptorHeap[ResIdIdsBuffer];
	Output.entityId = idsBuffer[Input.instanceID];
#else
	Output.position = mul(mul(Input.position, WorldMatrix), ViewProjectionMatrix);
#endif

    return Output;
}

uint PSMain(VS_OUTPUT input) : SV_TARGET
{
#ifdef INSTANCED
	return input.entityId;
#else
	return EntityId;
#endif
}
