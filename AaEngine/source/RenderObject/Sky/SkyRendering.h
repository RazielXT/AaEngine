#pragma once

#include "Resources/Model/VertexBufferModel.h"

class RenderWorld;
class MaterialResources;
class RenderEntity;

namespace DirectX
{
	class ResourceUploadBatch;
}

class SkyRendering
{
public:

	void createSky(RenderWorld& renderWorld, MaterialResources& materials, DirectX::ResourceUploadBatch& batch);
	void createClouds(RenderWorld& renderWorld, MaterialResources& materials, ID3D12Device* device, DirectX::ResourceUploadBatch& batch);
	void createMoon(RenderWorld& renderWorld, MaterialResources& materials, DirectX::ResourceUploadBatch& batch);

private:

	VertexBufferModel cloudsModel;
};
