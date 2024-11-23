#include "GenerateMipsComputeShader.h"
#include "DescriptorManager.h"
#include "Directx.h"
#include "../Src/d3dx12.h"

void GenerateMipsComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, TextureResource& texture, DescriptorManager& mgr)
{
	auto resource = texture.texture.Get();

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	ID3D12DescriptorHeap* ppHeaps[] = { mgr.mainDescriptorHeap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	TextureResource::TransitionState(commandList, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	//src Texture
	commandList->SetComputeRootDescriptorTable(1, texture.textureView.srvHandles);

	UINT size = texture.width;

	struct
	{
		UINT SrcMipIndex;
		DirectX::XMFLOAT3 InvOutTexelSize;
	}
	constants;

	for (UINT mip = 1; mip < texture.uav.size(); mip++)
	{
		size /= 2;

		auto uavTransition = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, mip);
		commandList->ResourceBarrier(1, &uavTransition);

		//target UAV
		commandList->SetComputeRootDescriptorTable(2, texture.uav[mip].uavHandles);
		constants.SrcMipIndex = mip - 1;
		constants.InvOutTexelSize = DirectX::XMFLOAT3(1 / float(size), 1 / float(size), 1 / float(size));

		commandList->SetComputeRoot32BitConstants(0, sizeof(constants) / sizeof(float), &constants,	0);

		UINT threads = max(size / 8, 1u);
		commandList->Dispatch(threads, threads, threads);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(resource);
		commandList->ResourceBarrier(1, &uavb);

		auto srvTransition = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, mip);
		commandList->ResourceBarrier(1, &srvTransition);
	}
}
