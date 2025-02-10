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
    float4 normal : NORMAL;
#ifdef INSTANCED
	uint instanceID : SV_InstanceID;
#endif
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 normal : TEXCOORD2;
	float4 worldPosition : TEXCOORD1;
#ifdef INSTANCED
	uint entityId : TEXCOORD0;
#endif
};

VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;

#ifdef INSTANCED
	Output.worldPosition = mul(Input.position, InstancingBuffer[Input.instanceID]);
	
	StructuredBuffer<uint> idsBuffer = ResourceDescriptorHeap[ResIdIdsBuffer];
	Output.entityId = idsBuffer[Input.instanceID];
#else
	Output.worldPosition = mul(Input.position, WorldMatrix);
#endif

    Output.position = mul(Output.worldPosition, ViewProjectionMatrix);
	Output.normal = Input.normal;

    return Output;
}

struct PSOutputId
{
    uint4 id : SV_Target0;
    float4 position : SV_Target1;
    float4 normal : SV_Target2;
};

PSOutputId PSMain(VS_OUTPUT input)
{
	PSOutputId output;

#ifdef INSTANCED		
	uint entityId = input.entityId;
#else
	uint entityId =  EntityId;
#endif

	output.id = uint4(entityId, 0, 0, 0);
	output.position = float4(input.worldPosition.xyz, 0);
	output.normal = float4(input.normal.xyz, 0);
	return output;
}
