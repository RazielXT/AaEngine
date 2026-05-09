#include "IndirectCommandsCS.h"

void IndirectDrawIndexedClearCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT commandCount, ID3D12Resource* commands)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, 1, &commandCount, 0);
	commandList->SetComputeRootUnorderedAccessView(1, commands->GetGPUVirtualAddress());

	commandList->Dispatch((commandCount + 63) / 64, 1, 1);
}
