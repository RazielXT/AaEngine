#pragma once

#include "ComputeShader.h"

struct TextureResource;
class DescriptorManager;

class GenerateMipsComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, TextureResource& texture, DescriptorManager& mgr, UINT frameIndex);
};