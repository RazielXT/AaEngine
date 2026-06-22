#pragma once

#include "ComputeShader.h"

class IndirectDrawIndexedClearCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT commandCount, ID3D12Resource* commands);
	// Clears commandCount entries starting at commandsAddr (lets a contiguous slice of a shared buffer be cleared).
	void dispatch(ID3D12GraphicsCommandList* commandList, UINT commandCount, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr);
};
