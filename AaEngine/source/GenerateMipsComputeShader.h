#pragma once

#include "ComputeShader.h"

class GpuTexture3D;
class ShaderTextureViewUAV;

class Generate3DMips3xCS : public ComputeShader
{
public:

	Generate3DMips3xCS();

	void dispatch(ID3D12GraphicsCommandList* commandList, GpuTexture3D& texture);
};

class GenerateNormalMips4xCS : public ComputeShader
{
public:

	GenerateNormalMips4xCS() = default;

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT textureSize, const std::vector<ShaderTextureViewUAV>& uav);
};
