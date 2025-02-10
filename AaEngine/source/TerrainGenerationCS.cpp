#include "TerrainGenerationCS.h"

void TerrainGenerationComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* vertexBuffer, UINT heighmapId, UINT worldGridSize, XMINT2 worldGridOffset, UINT gridSize, UINT gridScale, XMUINT4 borderSeams, float quadUnitSize, float maxHeight, float worldMappingScale)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct
	{
		XMUINT4 BorderBlendIdx;
		XMINT2 WorldGridOffset;
		float InvWorldGridSize;
		UINT GridScale; // lod0 = 1, lod2 = 2, lod3 = 4 etc
		UINT GridSize;	//65
		float HeightSize;
		float QuadUnitSize;
		float WorldMappingScale;
		UINT TexIdTerrainDepth;
	}
	data = {
		borderSeams, worldGridOffset, 1.f / worldGridSize, gridScale, gridSize, maxHeight, quadUnitSize, worldMappingScale, heighmapId
	};

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	commandList->SetComputeRootUnorderedAccessView(1, vertexBuffer->GetGPUVirtualAddress());

	commandList->Dispatch(4, 4, 1);
}
