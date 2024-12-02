#pragma once

#include "ShaderConstantBuffers.h"
#include "AaMath.h"
#include <vector>

struct RenderObjectsVisibilityData;
class AaEntity;
class AaCamera;
class RenderTargetTexture;
class RenderTargetTextures;
class FrameParameters;

class ShaderConstantsProvider
{
public:

	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const AaCamera& camera, const RenderTargetTexture& target);
	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const AaCamera& camera, const RenderTargetTextures& targets);

	XMFLOAT2 inverseViewportSize;
	XMUINT2 viewportSize;

	AaEntity* entity{};

	XMFLOAT4X4 getWvpMatrix() const;
	XMMATRIX getWorldMatrix() const;
	XMMATRIX getViewProjectionMatrix() const;
	XMMATRIX getViewMatrix() const;
	XMMATRIX getInverseViewProjectionMatrix() const;
	XMMATRIX getInverseViewMatrix() const;
	XMMATRIX getInverseProjectionMatrix() const;

	XMFLOAT3 getWorldPosition() const;
	XMFLOAT3 getCameraPosition() const;

	D3D12_GPU_VIRTUAL_ADDRESS getGeometryBuffer() const;

	const RenderObjectsVisibilityData& info;

	const AaCamera& camera;

	const FrameParameters& params;
};