#pragma once

#include "ComputeShader.h"

class TextureToMeshCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT terrain, UINT w, UINT h, float scale, ID3D12Resource* vertexBuffer);
};

class WaterTextureToMeshCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT water, UINT w, UINT h, ID3D12Resource* vertexBuffer);
};
