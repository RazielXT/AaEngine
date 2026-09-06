#include "Scene/InstancePlacement.h"
#include "Resources/Shader/ShaderDataBuffers.h"

HeightmapPlacementShaderDescription CreateSlopePlacementShader(const SlopePlacementParams& params)
{
	HeightmapPlacementShaderDescription description;
	description.name = "slopePlacementCS";
	description.shader = { "instancing/slopePlacementCS.hlsl", "main", "cs_6_6" };
	description.constants.resize(sizeof(params) / sizeof(UINT));
	memcpy(description.constants.data(), &params, sizeof(params));
	return description;
}

void HeightmapPlacementComputeShader::dispatch(
	ID3D12GraphicsCommandList* commandList,
	const Input& input,
	const std::vector<UINT>& customConstants,
	D3D12_GPU_VIRTUAL_ADDRESS instanceBuffer,
	D3D12_GPU_VIRTUAL_ADDRESS commandBuffer)
{
	if (!customConstantsBuffer)
	{
		customConstantsBuffer = ShaderDataBuffers::get().CreateUploadStructuredBuffer(
			customConstants.data(), (UINT)customConstants.size() * sizeof(UINT));
		customConstantsBuffer->SetName(L"InstancePlacementConstants");
	}

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);
	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(UINT), &input, 0);
	commandList->SetComputeRootConstantBufferView(1, customConstantsBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, instanceBuffer);
	commandList->SetComputeRootUnorderedAccessView(3, commandBuffer);
	commandList->Dispatch(1, 1, 1);
}
