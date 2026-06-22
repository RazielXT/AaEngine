#include "Resources/Compute/ClearBufferCS.h"

void ClearBufferComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* buffer, UINT size)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRootUnorderedAccessView(0, buffer->GetGPUVirtualAddress());

	const UINT threads = 256;
	const UINT clearPerThread = 4;
	commandList->Dispatch(size / (clearPerThread * threads), 1, 1);
}

void ClearTextureComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const ShaderTextureView& texture, DirectX::XMUINT3 size)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRootDescriptorTable(0, texture.uavHandle);

	const UINT threads = 4;
	commandList->Dispatch(size.x / threads, size.y / threads, size.z / threads);
}
