#pragma once

#include "ComputeShader.h"

struct TextureResource;
class DescriptorManager;

class GenerateMipsComputeShader : public ComputeShader
{
public:

	GenerateMipsComputeShader();

	void dispatch(ID3D12GraphicsCommandList* commandList, TextureResource& texture, DescriptorManager& mgr);
};