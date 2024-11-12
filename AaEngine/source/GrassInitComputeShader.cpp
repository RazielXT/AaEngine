#include "GrassInitComputeShader.h"
#include "DescriptorManager.h"
#include "../Src/d3dx12.h"
#include "GrassArea.h"

void GrassInitComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, GrassAreaDescription& desc, XMMATRIX invView, UINT colorTex, UINT depthTex, ID3D12Resource* vertexBuffer, ID3D12Resource* vertexCounter, UINT frameIndex)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	ID3D12DescriptorHeap* ppHeaps[] = { DescriptorManager::get().mainDescriptorHeap[frameIndex] };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	struct
	{
		XMMATRIX invViewMatrix;
		float spacing;
		XMFLOAT3 boundsMin;
		XMFLOAT3 boundsMax;
		float width;
		UINT count;
		UINT rows;
		UINT depthTexture;
		UINT colorTexture;
	}
	ctx = {
		invView,
		desc.spacing,
		desc.bbox.Center - desc.bbox.Extents,
		desc.bbox.Center + desc.bbox.Extents,
		desc.width, desc.count, desc.areaCount.y,
		depthTex, colorTex
	};

	commandList->SetComputeRoot32BitConstants(0, sizeof(ctx) / sizeof(float), &ctx, 0);
	commandList->SetComputeRootUnorderedAccessView(1, vertexBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, vertexCounter->GetGPUVirtualAddress());

	UINT threads = (ctx.count + 127) / 128;
	commandList->Dispatch(threads, 1, 1);
}
