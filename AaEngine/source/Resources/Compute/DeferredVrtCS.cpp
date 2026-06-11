#include "Resources/Compute/DeferredVrtCS.h"

void DeferredVrtGenerateRaysCS::dispatch(ID3D12GraphicsCommandList* commandList, const DispatchParams& params, ID3D12Resource* outputRays, ID3D12Resource* rayResults, ID3D12Resource* queueState, ID3D12Resource* dispatchArgs)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(params) / sizeof(UINT), &params, 0);
	commandList->SetComputeRootUnorderedAccessView(1, outputRays->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, rayResults->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(3, queueState->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(4, dispatchArgs->GetGPUVirtualAddress());

	commandList->Dispatch((params.ViewportSize.x + 7) / 8, (params.ViewportSize.y + 7) / 8, 1);
}

void DeferredVrtTraceRayCS::dispatchIndirect(ID3D12GraphicsCommandList* commandList, ID3D12CommandSignature* commandSignature, const DispatchParams& params, D3D12_GPU_VIRTUAL_ADDRESS voxelInfo, ID3D12Resource* inputRays, ID3D12Resource* outputRays, ID3D12Resource* rayResults, ID3D12Resource* queueState, ID3D12Resource* outputDispatchArgs, ID3D12Resource* indirectDispatchArgs)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(params) / sizeof(UINT), &params, 0);
	commandList->SetComputeRootConstantBufferView(1, voxelInfo);
	commandList->SetComputeRootShaderResourceView(2, inputRays->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(3, outputRays->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(4, rayResults->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(5, queueState->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(6, outputDispatchArgs->GetGPUVirtualAddress());

	commandList->ExecuteIndirect(commandSignature, 1, indirectDispatchArgs, 0, nullptr, 0);
}

void DeferredVrtCollectRaysCS::dispatch(ID3D12GraphicsCommandList* commandList, const DispatchParams& params, ID3D12Resource* rayResults)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(params) / sizeof(UINT), &params, 0);
	commandList->SetComputeRootShaderResourceView(1, rayResults->GetGPUVirtualAddress());

	commandList->Dispatch((params.ViewportSize.x + 7) / 8, (params.ViewportSize.y + 7) / 8, 1);
}

void DeferredVrtResetQueueCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT queueIndex, ID3D12Resource* queueState, ID3D12Resource* dispatchArgs)
{
	DispatchParams params{};
	params.QueueIndex = queueIndex;

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(params) / sizeof(UINT), &params, 0);
	commandList->SetComputeRootUnorderedAccessView(1, queueState->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, dispatchArgs->GetGPUVirtualAddress());

	commandList->Dispatch(1, 1, 1);
}