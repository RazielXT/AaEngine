#pragma once

#include "ComputeShader.h"
#include "MathUtils.h"

class TerrainGenerationComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* vertexBuffer, UINT heighmapId, UINT worldGridSize, XMINT2 worldGridOffset, UINT gridSize, UINT gridScale, XMUINT4 borderSeams, float worldSize, float maxHeight, float worldScale);
};

class TerrainHeightmapCS : public ComputeShader
{
public:

	struct Input
	{
		XMINT2 offset;
		UINT noiseTex;
	};
	void dispatch(ID3D12GraphicsCommandList* commandList, UINT w, UINT h, D3D12_GPU_DESCRIPTOR_HANDLE output, Input);
};
