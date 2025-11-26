#include "CopyTexturesCS.h"

void CopyTexturesCS::dispatch(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE input, UINT w, UINT h, D3D12_GPU_DESCRIPTOR_HANDLE output)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct Input
	{
		UINT width;
		UINT height;
	}
	data = { w, h };

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	commandList->SetComputeRootDescriptorTable(1, input);
	commandList->SetComputeRootDescriptorTable(2, output);

	commandList->Dispatch(data.width / 8, data.height / 8, 1);
}
