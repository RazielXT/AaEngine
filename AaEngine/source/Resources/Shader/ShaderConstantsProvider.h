#pragma once

#include "Resources/Shader/ShaderDataBuffers.h"
#include "Resources/Textures/TextureResources.h"
#include "Utils/MathUtils.h"

#include "Scene/FrameParameters.h"

struct RenderObjectsVisibilityData;
class RenderEntity;
class Camera;
class GpuTexture2D;
class RenderTargetTexturesView;

enum RenderViewId
{
	RenderViewId_Default,
	RenderViewId_ShadowCascade0,
	RenderViewId_ShadowCascade1,
	RenderViewId_Voxelize0,
	RenderViewId_Voxelize1,
	RenderViewId_Voxelize2,
	RenderViewId_Voxelize3,
	RenderViewId_VoxelizeShadowMap,
	RenderViewId_Count
};

class ShaderConstantsProvider
{
public:

	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const Camera& camera, const GpuTexture2D& target);
	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const Camera& camera, const Camera& mainCamera, const GpuTexture2D& target);
	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const Camera& camera, const RenderTargetTexturesView& targets);

	XMFLOAT2 inverseViewportSize;
	XMUINT2 viewportSize;

	RenderEntity* entity{};
	ID3D12Resource* uavBarrier{};

	RenderViewId viewId{};

	XMMATRIX getWorldMatrix() const;
	XMMATRIX getPreviousWorldMatrix() const;
	XMMATRIX getViewProjectionMatrix() const;
	XMMATRIX getViewMatrix() const;
	XMMATRIX getProjectionMatrix() const;
	XMMATRIX getInverseViewProjectionMatrix() const;
	XMMATRIX getInverseViewMatrix() const;
	XMMATRIX getInverseProjectionMatrix() const;

	XMFLOAT3 getWorldPosition() const;
	XMFLOAT3 getCameraPosition() const;
	XMFLOAT3 getCameraDirection() const;
	XMFLOAT3 getMainCameraPosition() const;
	XMFLOAT3 getMainCameraDirection() const;

	D3D12_GPU_VIRTUAL_ADDRESS getGeometryBuffer() const;
	D3D12_GPU_VIRTUAL_ADDRESS getGeometryRedirectBuffer() const;

	const RenderObjectsVisibilityData& info;

	const Camera& camera;
	const Camera& mainCamera;

	const FrameParameters& params;
};