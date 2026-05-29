#pragma once

#include "Resources/Model/VertexBufferModel.h"
#include "Resources/Shader/ShaderDataBuffers.h"
#include "Scene/FrameParameters.h"

class RenderWorld;
class MaterialResources;
class RenderEntity;
struct GraphicsResources;

namespace DirectX
{
	class ResourceUploadBatch;
}

class SkyRendering
{
public:

	void initializeSkyParameters(SkyParameters& params, ID3D12Device* device, GraphicsResources& resources, DirectX::ResourceUploadBatch& batch);
	void updateSkyParameters(const SkyParameters& params, UINT frameIndex);

	void createSky(RenderWorld& renderWorld, MaterialResources& materials, DirectX::ResourceUploadBatch& batch);
	void createClouds(RenderWorld& renderWorld, MaterialResources& materials, ID3D12Device* device, DirectX::ResourceUploadBatch& batch);
	void createMoon(RenderWorld& renderWorld, MaterialResources& materials, DirectX::ResourceUploadBatch& batch);

private:

	VertexBufferModel cloudsModel;

	CbufferView skyParamsCbuffer;
};
