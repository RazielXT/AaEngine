#include "ShaderConstantsProvider.h"
#include "RenderQueue.h"
#include "RenderObject.h"

ShaderConstantsProvider::ShaderConstantsProvider(const FrameParameters& p, const RenderObjectsVisibilityData& i, const Camera& c, const RenderTargetTexture& target) : info(i), camera(c), params(p)
{
	inverseViewportSize = { 1.f / target.width, 1.f / target.height };
	viewportSize = { target.width, target.height };
}

ShaderConstantsProvider::ShaderConstantsProvider(const FrameParameters& params, const RenderObjectsVisibilityData& info, const Camera& camera, const RenderTargetTextures& targets) :
	ShaderConstantsProvider(params, info, camera, targets.textures.front())
{
}

XMMATRIX ShaderConstantsProvider::getWorldMatrix() const
{
	return entity->getWorldMatrix();
}

XMMATRIX ShaderConstantsProvider::getPreviousWorldMatrix() const
{
	return entity->getPreviousWorldMatrix();
}

XMMATRIX ShaderConstantsProvider::getViewProjectionMatrix() const
{
	return camera.getViewProjectionMatrix();
}

XMMATRIX ShaderConstantsProvider::getViewMatrix() const
{
	return camera.getViewMatrix();
}

XMMATRIX ShaderConstantsProvider::getProjectionMatrix() const
{
	return camera.getProjectionMatrixNoOffset();
}

XMMATRIX ShaderConstantsProvider::getInverseViewProjectionMatrix() const
{
	return XMMatrixInverse(nullptr, getViewProjectionMatrix());
}

XMMATRIX ShaderConstantsProvider::getInverseViewMatrix() const
{
	return XMMatrixInverse(nullptr, camera.getViewMatrix());
}

XMMATRIX ShaderConstantsProvider::getInverseProjectionMatrix() const
{
	return XMMatrixInverse(nullptr, camera.getProjectionMatrix());
}

XMFLOAT3 ShaderConstantsProvider::getWorldPosition() const
{
	return entity->getPosition();
}

XMFLOAT3 ShaderConstantsProvider::getCameraPosition() const
{
	return camera.getPosition();
}

D3D12_GPU_VIRTUAL_ADDRESS ShaderConstantsProvider::getGeometryBuffer() const
{
	return entity->geometry.geometryCustomBuffer;
}
