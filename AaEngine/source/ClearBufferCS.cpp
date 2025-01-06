#include "ClearBufferCS.h"

void ClearBufferComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* buffer, UINT size)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRootUnorderedAccessView(0, buffer->GetGPUVirtualAddress());

	const UINT threads = 256;
	commandList->Dispatch(size / threads, 1, 1);
}
