#pragma once

#include "Resources/Model/VertexBufferModel.h"

class SceneManager;
class MaterialResources;
class SceneEntity;

namespace DirectX
{
	class ResourceUploadBatch;
}

class SkyRendering
{
public:

	void createClouds(SceneManager& sceneMgr, MaterialResources& materials, ID3D12Device* device, DirectX::ResourceUploadBatch& batch);
	void createMoon(SceneManager& sceneMgr, MaterialResources& materials, DirectX::ResourceUploadBatch& batch);

private:

	VertexBufferModel cloudsModel;
};
