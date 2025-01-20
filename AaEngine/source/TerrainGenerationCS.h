#pragma once

#include "ComputeShader.h"
#include "MathUtils.h"

class TerrainGenerationComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* vertexBuffer, UINT heighmapId, UINT gridSize, Vector2 offset, XMUINT2 gridIdx, float scale, XMUINT4 borderSeams, float worldSize);
};
