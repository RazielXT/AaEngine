#include "Resources/Compute/WaterInteractionCS.h"

void WaterInteractionCS::dispatch(ID3D12GraphicsCommandList* commandList, DispatchParams params, ID3D12Resource* inputBuffer, ID3D12Resource* outputBuffer)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(params) / sizeof(UINT), &params, 0);
	commandList->SetComputeRootShaderResourceView(1, inputBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, outputBuffer->GetGPUVirtualAddress());

	commandList->Dispatch((params.queryCount + 63) / 64, 1, 1);
}
