#pragma once

#include "ShaderDataBuffers.h"
#include "TextureResources.h"
#include "MathUtils.h"

struct RenderObjectsVisibilityData;
class SceneEntity;
class Camera;
class GpuTexture2D;
class RenderTargetTexturesView;
struct FrameParameters;

class ShaderConstantsProvider
{
public:

	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const Camera& camera, const GpuTexture2D& target);
	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const Camera& camera, const Camera& mainCamera, const GpuTexture2D& target);
	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const Camera& camera, const RenderTargetTexturesView& targets);

	XMFLOAT2 inverseViewportSize;
	XMUINT2 viewportSize;

	SceneEntity* entity{};
	ID3D12Resource* uavBarrier{};

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
	XMFLOAT3 getMainCameraPosition() const;

	D3D12_GPU_VIRTUAL_ADDRESS getGeometryBuffer() const;

	const RenderObjectsVisibilityData& info;

	const Camera& camera;
	const Camera& mainCamera;

	const FrameParameters& params;
};