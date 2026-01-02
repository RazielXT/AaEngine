#pragma once

#include "ComputeShader.h"
#include "MathUtils.h"

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

class WaterTextureToTextureCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT water, UINT terrain, UINT w, UINT h, D3D12_GPU_DESCRIPTOR_HANDLE outputNormal, D3D12_GPU_DESCRIPTOR_HANDLE outputHeight, Vector3 camPos);
};

class GenerateHeightmapNormalsCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT terrain, UINT normals, UINT w, UINT h, float heightScale, float worldSize);
};
