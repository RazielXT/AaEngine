#include "GenerateMipsComputeShader.h"
#include "DescriptorManager.h"
#include "Directx.h"
#include "directx\d3dx12.h"

Generate3DMips3xCS::Generate3DMips3xCS()
{
	volatileTextures = true;
}

void Generate3DMips3xCS::dispatch(ID3D12GraphicsCommandList* commandList, GpuTexture3D& texture)
{
	auto resource = texture.texture.Get();

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRootDescriptorTable(1, texture.view.srvHandles);

	struct
	{
		UINT SrcMipIndex;
		DirectX::XMFLOAT3 InvOutTexelSize;
	}
	constants;

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);

	UINT outSize = texture.width / 2;

	// 3 mips per dispatch
	for (UINT mip = 1; (mip + 2) < texture.uav.size(); mip += 3)
	{
		//target UAV
		commandList->SetComputeRootDescriptorTable(2, texture.uav[mip].uavHandles);
		commandList->SetComputeRootDescriptorTable(3, texture.uav[mip + 1].uavHandles);
		commandList->SetComputeRootDescriptorTable(4, texture.uav[mip + 2].uavHandles);

		constants.SrcMipIndex = mip - 1;
		constants.InvOutTexelSize = DirectX::XMFLOAT3(1 / float(outSize), 1 / float(outSize), 1 / float(outSize));
		commandList->SetComputeRoot32BitConstants(0, sizeof(constants) / sizeof(float), &constants,	0);

		const UINT groups = max(outSize / 4, 1u);
		commandList->Dispatch(groups, groups, groups);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(resource);
		commandList->ResourceBarrier(1, &uavb);

		outSize /= 8;
	}

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
}

void GenerateNormalMips4xCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT textureSize, const std::vector<ShaderUAV>& uav)
{
	if (uav.size() < 5)
		return;

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct
	{
		float invSize;
		UINT SrcMipIndex;
		UINT DstMipIndex1;
		UINT DstMipIndex2;
		UINT DstMipIndex3;
		UINT DstMipIndex4;
	}
	constants = { 1.f / textureSize, uav[0].heapIndex, uav[1].heapIndex, uav[2].heapIndex, uav[3].heapIndex, uav[4].heapIndex };

	commandList->SetComputeRoot32BitConstants(0, sizeof(constants) / sizeof(float), &constants, 0);

	const UINT threads = 8;
	const UINT groups = max(textureSize / (threads * 2), 1u);
	commandList->Dispatch(groups, groups, 1);
}
