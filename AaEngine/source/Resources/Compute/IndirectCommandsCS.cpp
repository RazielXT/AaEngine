#include "IndirectCommandsCS.h"

void IndirectDrawIndexedClearCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT commandCount, ID3D12Resource* commands)
{
	dispatch(commandList, commandCount, commands->GetGPUVirtualAddress());
}

void IndirectDrawIndexedClearCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT commandCount, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, 1, &commandCount, 0);
	commandList->SetComputeRootUnorderedAccessView(1, commandsAddr);

	commandList->Dispatch((commandCount + 63) / 64, 1, 1);
}
