#include "AoPrepareDepthBuffersCS.h"
#include "DescriptorManager.h"

void AoPrepareDepthBuffersCS::dispatch(UINT width, UINT height, const ShaderTextureView& input, ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	commandList->SetComputeRootDescriptorTable(1, input.srvHandle);

	const uint32_t bufferWidth = (width + 15) / 16;
	const uint32_t bufferHeight = (height + 15) / 16;

	commandList->Dispatch(bufferWidth, bufferHeight, 1);
}

void AoPrepareDepthBuffers2CS::dispatch(UINT width, UINT height, const ShaderTextureView& input, ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	commandList->SetComputeRootDescriptorTable(1, input.srvHandle);

	const uint32_t bufferWidth = (width + 15) / 16;
	const uint32_t bufferHeight = (height + 15) / 16;

	commandList->Dispatch(bufferWidth, bufferHeight, 1);
}
