#pragma once

#include "ShaderConstantBuffers.h"
#include "AaMath.h"
#include <vector>

struct RenderInformation;
class AaEntity;
class AaCamera;
class RenderTargetInfo;

class ShaderConstantsProvider
{
public:

	ShaderConstantsProvider(const RenderInformation& info, const AaCamera& camera, const RenderTargetInfo& target);

	DirectX::XMFLOAT2 inverseViewportSize;

	std::vector<std::vector<float>> buffers;

	XMFLOAT4X4 getWvpMatrix() const;
	XMMATRIX getWorldMatrix() const;
	XMMATRIX getViewProjectionMatrix() const;
	XMMATRIX getInverseViewProjectionMatrix() const;

	XMFLOAT3 getWorldPosition() const;
	XMFLOAT3 getCameraPosition() const;

	D3D12_GPU_VIRTUAL_ADDRESS getGeometryBuffer() const;

	AaEntity* entity{};

	const RenderInformation& info;

	const AaCamera& camera;
};