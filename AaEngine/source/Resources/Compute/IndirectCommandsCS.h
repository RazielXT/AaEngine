#pragma once

#include "ComputeShader.h"

class IndirectDrawIndexedClearCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT commandCount, ID3D12Resource* commands);
};
