#include "TextureToMeshCS.h"

void TextureToMeshCS::dispatch(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE texture, UINT w, UINT h, ID3D12Resource* vertexBuffer)
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
	commandList->SetComputeRootDescriptorTable(2, texture);
	commandList->SetComputeRootUnorderedAccessView(1, vertexBuffer->GetGPUVirtualAddress());

	commandList->Dispatch(data.width / 8, data.height / 8, 1);
}
