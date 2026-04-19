#pragma once

#include "Resources/Compute/ComputeShader.h"
#include "Utils/MathUtils.h"

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
