#include "ShaderConstantsProvider.h"
#include "RenderQueue.h"
#include "AaRenderables.h"

ShaderConstantsProvider::ShaderConstantsProvider(const RenderInformation& i, const AaCamera& c) : info(i), camera(c)
{
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

