#pragma once

#include "ComputeShader.h"

class TextureToMeshCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE texture, UINT w, UINT h, ID3D12Resource* vertexBuffer);
};
