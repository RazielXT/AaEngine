#include "ShaderConstantsProvider.h"
#include "RenderQueue.h"
#include "RenderObject.h"

ShaderConstantsProvider::ShaderConstantsProvider(const FrameParameters& p, const RenderObjectsVisibilityData& i, const AaCamera& c, const RenderTargetInfo& target) : info(i), camera(c), params(p)
{
	inverseViewportSize = { 1.f / target.width, 1.f / target.height };
}

XMFLOAT4X4 ShaderConstantsProvider::getWvpMatrix() const
{
	return entity->getWvpMatrix(info.wvpMatrix);
}

XMMATRIX ShaderConstantsProvider::getWorldMatrix() const
{
	return entity->getWorldMatrix();
}

XMMATRIX ShaderConstantsProvider::getViewProjectionMatrix() const
{
	return camera.getViewProjectionMatrix();
}

XMMATRIX ShaderConstantsProvider::getInverseViewProjectionMatrix() const
{
	return XMMatrixInverse(nullptr, getViewProjectionMatrix());
}

DirectX::XMFLOAT3 ShaderConstantsProvider::getWorldPosition() const
{
	return entity->getPosition();
}

DirectX::XMFLOAT3 ShaderConstantsProvider::getCameraPosition() const
{
	return camera.getPosition();
}

D3D12_GPU_VIRTUAL_ADDRESS ShaderConstantsProvider::getGeometryBuffer() const
{
	return entity->geometry.geometryCustomBuffer;
}

