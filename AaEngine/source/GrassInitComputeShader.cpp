#include "GrassInitComputeShader.h"
#include "DescriptorManager.h"
#include "directx\d3dx12.h"
#include "GrassArea.h"

void GrassInitComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, GrassAreaDescription& desc, XMMATRIX invView, UINT colorTex, UINT depthTex, ID3D12Resource* vertexBuffer, ID3D12Resource* vertexCounter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct
	{
		XMFLOAT4X4 invViewMatrix;
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
		{},
		desc.spacing,
		desc.bbox.Center - desc.bbox.Extents,
		desc.bbox.Center + desc.bbox.Extents,
		desc.width, desc.count, desc.areaCount.y,
		depthTex, colorTex
	};

	XMStoreFloat4x4(&ctx.invViewMatrix, invView);

	commandList->SetComputeRoot32BitConstants(0, sizeof(ctx) / sizeof(float), &ctx, 0);
	commandList->SetComputeRootUnorderedAccessView(1, vertexBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, vertexCounter->GetGPUVirtualAddress());

	UINT threads = (ctx.count + 127) / 128;
	commandList->Dispatch(threads, 1, 1);
}
