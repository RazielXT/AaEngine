#pragma once

#include "ComputeShader.h"

class CopyTexturesCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE input, UINT w, UINT h, D3D12_GPU_DESCRIPTOR_HANDLE output);
};
