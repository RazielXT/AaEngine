#pragma once

#include "ComputeShader.h"

class TextureToMeshCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT terrain, UINT water, UINT w, UINT h, ID3D12Resource* vertexBuffer);
};
