RWStructuredBuffer<uint4> buffer : register(u0);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	buffer[DTid.x] = uint4(0,0,0,0);
}
