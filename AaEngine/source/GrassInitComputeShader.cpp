#include "GrassInitComputeShader.h"
#include "ResourcesManager.h"
#include "../Src/d3dx12.h"
#include "GrassArea.h"

void GrassInitComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, GrassArea& grass, UINT colorTexture, UINT depthTexture, XMMATRIX invVPMatrix, ResourcesManager& mgr, UINT frameIndex)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	ID3D12DescriptorHeap* ppHeaps[] = { mgr.mainDescriptorHeap[frameIndex] };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	struct
	{
		XMMATRIX invVPMatrix;
		XMFLOAT3 boundsMin;
		float padding;
		XMFLOAT3 boundsMax;
		UINT count;
		UINT rows;
		UINT depthTexture;
		UINT colorTexture;
	}
	constants = { invVPMatrix, grass.bbox.Center - grass.bbox.Extents, {}, grass.bbox.Center + grass.bbox.Extents, grass.count, grass.areaCount.y, depthTexture, colorTexture };

	commandList->SetComputeRoot32BitConstants(0, sizeof(constants) / sizeof(float), &constants, 0);
	commandList->SetComputeRootUnorderedAccessView(1, grass.gpuBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, grass.vertexCounter->GetGPUVirtualAddress());

	UINT threads = (grass.count + 127) / 128;
	commandList->Dispatch(threads, 1, 1);
}
