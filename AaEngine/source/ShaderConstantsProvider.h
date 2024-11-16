#pragma once

#include "ShaderConstantBuffers.h"
#include "AaMath.h"
#include <vector>

struct RenderObjectsVisibilityData;
class AaEntity;
class AaCamera;
class RenderTargetInfo;
class FrameParameters;

class ShaderConstantsProvider
{
public:

	ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const AaCamera& camera, const RenderTargetInfo& target);

	DirectX::XMFLOAT2 inverseViewportSize;

	AaEntity* entity{};

	XMFLOAT4X4 getWvpMatrix() const;
	XMMATRIX getWorldMatrix() const;
	XMMATRIX getViewProjectionMatrix() const;
	XMMATRIX getInverseViewProjectionMatrix() const;

	XMFLOAT3 getWorldPosition() const;
	XMFLOAT3 getCameraPosition() const;

	D3D12_GPU_VIRTUAL_ADDRESS getGeometryBuffer() const;

	const RenderObjectsVisibilityData& info;

	const AaCamera& camera;

	const FrameParameters& params;
};