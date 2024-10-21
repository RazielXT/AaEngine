#pragma once

#include "ComputeShader.h"

struct TextureResource;
class ResourcesManager;

class GenerateMipsComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, TextureResource& texture, ResourcesManager& mgr, UINT frameIndex);
};