#include "TerrainGenerationCS.h"

void TerrainHeightmapCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT w, UINT h, D3D12_GPU_DESCRIPTOR_HANDLE output, Input input)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootDescriptorTable(1, output);

	commandList->Dispatch(w / 8, h / 8, 1);
}
