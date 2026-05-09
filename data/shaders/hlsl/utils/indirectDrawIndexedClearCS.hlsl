uint CommandCount;

RWByteAddressBuffer drawCommandsBuffer : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint idx = dispatchThreadID.x;
	if (idx >= CommandCount)
		return;

	// Each D3D12_DRAW_INDEXED_ARGUMENTS is 20 bytes (5 uints)
	// InstanceCount is at offset 4 bytes within each entry
	drawCommandsBuffer.Store(idx * 20 + 4, 0);
}
