#pragma once

#include "ComputeShader.h"
#include "MathUtils.h"

class TerrainGenerationComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* vertexBuffer, UINT heighmapId, UINT worldGridSize, XMINT2 worldGridOffset, UINT gridSize, UINT gridScale, XMUINT4 borderSeams, float worldSize, float maxHeight, float worldScale);
};
