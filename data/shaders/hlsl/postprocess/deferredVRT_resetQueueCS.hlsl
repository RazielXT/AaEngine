cbuffer CB0 : register(b0)
{
	uint QueueIndex;
	uint Padding0;
	uint Padding1;
	uint Padding2;
};

RWStructuredBuffer<uint> QueueState : register(u0);
RWStructuredBuffer<uint> DispatchArgs : register(u1);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	QueueState[QueueIndex] = 0;
	DispatchArgs[0] = 0;
	DispatchArgs[1] = 1;
	DispatchArgs[2] = 1;
}