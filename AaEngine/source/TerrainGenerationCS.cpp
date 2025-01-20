#include "TerrainGenerationCS.h"

void TerrainGenerationComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* vertexBuffer, UINT heighmapId, UINT gridSize, Vector2 offset, XMUINT2 gridIdx, float scale, XMUINT4 borderSeams, float worldSize)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct
	{
		XMUINT4 borderSeams;
		XMUINT2 gridIdx;
		UINT gridSize;
		UINT heightmapId;
		Vector2 offset;
		float GridScale;
		float WorldSize;
		float HeightSize;
	}
	data = {
		borderSeams, gridIdx, gridSize, heighmapId, offset, scale, worldSize, 350
	};

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	commandList->SetComputeRootUnorderedAccessView(1, vertexBuffer->GetGPUVirtualAddress());

	const UINT groups = ceil(gridSize / 16.f);
	commandList->Dispatch(groups, groups, 1);
}
