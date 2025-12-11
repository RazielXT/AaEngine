#pragma once

#include "ComputeShader.h"

class GpuTexture3D;
class DescriptorManager;

class GenerateMipsComputeShader : public ComputeShader
{
public:

	GenerateMipsComputeShader();

	void dispatch(ID3D12GraphicsCommandList* commandList, GpuTexture3D& texture);
};