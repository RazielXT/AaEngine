#pragma once

#include "Resources/Compute/ComputeShader.h"
#include "Utils/Directx.h"

class ClearBufferComputeShader : public ComputeShader
{
public:

	ClearBufferComputeShader() = default;

	void dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* buffer, UINT size);
};
