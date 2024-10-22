#include "GenerateMipsComputeShader.h"
#include "ResourcesManager.h"
#include "Directx.h"
#include "../Src/d3dx12.h"

void GenerateMipsComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, TextureResource& texture, ResourcesManager& mgr, UINT frameIndex)
{
	auto resource = texture.textures[frameIndex].Get();

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { mgr.mainDescriptorHeap[frameIndex] };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	auto b = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &b);

	//src Texture
	commandList->SetComputeRootDescriptorTable(1, texture.textureView.srvHandles[frameIndex]);

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

		auto uavTransition = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &uavTransition);

		//target UAV
		commandList->SetComputeRootDescriptorTable(2, texture.uav[mip].uavHandles[frameIndex]);
		constants.SrcMipIndex = mip - 1;
		constants.InvOutTexelSize = DirectX::XMFLOAT3(1 / float(size), 1 / float(size), 1 / float(size));

		commandList->SetComputeRoot32BitConstants(0, sizeof(constants) / sizeof(float), &constants,	0);

		UINT threads = max(size / 8, 1u);
		commandList->Dispatch(threads, threads, threads);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(resource);
		commandList->ResourceBarrier(1, &uavb);

		auto srvTransition = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &srvTransition);
	}

	b = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &b);
}
