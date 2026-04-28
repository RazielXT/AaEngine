#pragma once

#include "Resources/Compute/ComputeShader.h"

class GpuTexture3D;
class ShaderTextureViewUAV;

class Generate3DMips3xCS : public ComputeShader
{
public:

	Generate3DMips3xCS();

	void dispatch(ID3D12GraphicsCommandList* commandList, GpuTexture3D& texture);
};

class GenerateXYMips4xCS : public ComputeShader
{
public:

	GenerateXYMips4xCS() = default;

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT textureSize, const std::vector<ShaderTextureViewUAV>& uav);
};
