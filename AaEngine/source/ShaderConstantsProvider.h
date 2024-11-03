#pragma once

#include "ShaderConstantBuffers.h"
#include "AaMath.h"
#include <vector>

struct RenderInformation;
class AaEntity;
class AaCamera;

class ShaderConstantsProvider
{
public:

	ShaderConstantsProvider(const RenderInformation& info, const AaCamera& camera);

	std::vector<std::vector<float>> data;

	XMFLOAT4X4 getWvpMatrix() const;
	XMMATRIX getWorldMatrix() const;
	XMMATRIX getViewProjectionMatrix() const;

	XMFLOAT3 getCameraPosition() const;

	D3D12_GPU_VIRTUAL_ADDRESS getGeometryBuffer() const;

	AaEntity* entity{};

private:

	const RenderInformation& info;
	const AaCamera& camera;
};