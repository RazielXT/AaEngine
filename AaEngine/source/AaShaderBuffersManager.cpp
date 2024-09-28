#include "AaShaderBuffersManager.h"
#include "AaLogger.h"
#include "AaEntity.h"

AaShaderBuffersManager* instance = nullptr;

AaShaderBuffersManager::AaShaderBuffersManager(AaRenderSystem* mRS)
{
	if (instance)
		throw std::exception("duplicate AaShaderManager");

	mRenderSystem = mRS;
	instance = this;

	D3D11_BUFFER_DESC constDesc;
	ZeroMemory(&constDesc, sizeof(constDesc));
	constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constDesc.ByteWidth = sizeof(PerFrameConstants);
	constDesc.Usage = D3D11_USAGE_DEFAULT;
	HRESULT d3dResult = mRenderSystem->getDevice()->CreateBuffer(&constDesc, nullptr, &perFrameBuffer);

	ZeroMemory(&constDesc, sizeof(constDesc));
	constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constDesc.ByteWidth = sizeof(PerObjectConstants);
	constDesc.Usage = D3D11_USAGE_DEFAULT;
	d3dResult = mRenderSystem->getDevice()->CreateBuffer(&constDesc, nullptr, &perObjectBuffer);

	globalCbuffers["PerFrame"] = perFrameBuffer;
	globalCbuffers["PerObject"] = perObjectBuffer;
}

AaShaderBuffersManager::~AaShaderBuffersManager()
{
	for (auto b : globalCbuffers)
	{
		b.second->Release();
	}

	instance = nullptr;
}

AaShaderBuffersManager& AaShaderBuffersManager::get()
{
	return *instance;
}

void AaShaderBuffersManager::updatePerFrameConstants(float timeSinceLastFrame, AaCamera& cam, AaCamera& sun, AaSceneLight& light)
{
	perFrameConstantsBuffer.time_delta = timeSinceLastFrame;
	perFrameConstantsBuffer.time += timeSinceLastFrame;
	perFrameConstantsBuffer.ambientColour = light.ambientColor;

	perFrameConstantsBuffer.directional_light_direction = light.directionalLight.direction;
	perFrameConstantsBuffer.directional_light_color = light.directionalLight.color;

	perFrameConstantsBuffer.cameraPos = cam.getPosition();

	XMStoreFloat4x4(&perFrameConstantsBuffer.viewProjectionMatrix, cam.getViewProjectionMatrix());
	XMStoreFloat4x4(&perFrameConstantsBuffer.directional_light_VPMatrix, sun.getViewProjectionMatrix());

	mRenderSystem->getContext()->UpdateSubresource(perFrameBuffer, 0, nullptr, &perFrameConstantsBuffer, 0, 0);
}

void AaShaderBuffersManager::updatePerFrameConstants(AaCamera& cam)
{
	perFrameConstantsBuffer.cameraPos = cam.getPosition();
	XMStoreFloat4x4(&perFrameConstantsBuffer.viewProjectionMatrix, cam.getViewProjectionMatrix());

	mRenderSystem->getContext()->UpdateSubresource(perFrameBuffer, 0, nullptr, &perFrameConstantsBuffer, 0, 0);
}

void AaShaderBuffersManager::updateObjectConstants(AaEntity* ent, AaCamera& camera)
{
	perObjectConstantsBuffer.worldPosition = ent->getPosition();

	XMStoreFloat4x4(&perObjectConstantsBuffer.worldViewProjectionMatrix, XMMatrixTranspose(XMMatrixMultiply(ent->getWorldMatrix(), camera.getViewProjectionMatrix())));
	XMStoreFloat4x4(&perObjectConstantsBuffer.worldMatrix, XMMatrixTranspose(ent->getWorldMatrix()));

	mRenderSystem->getContext()->UpdateSubresource(perObjectBuffer, 0, nullptr, &perObjectConstantsBuffer, 0, 0);
}

void AaShaderBuffersManager::setCbuffer(const std::string& name, const float* data, size_t size)
{
	auto& cbuffer = globalCbuffers[name];

	if (!cbuffer)
	{
		D3D11_BUFFER_DESC constDesc;
		ZeroMemory(&constDesc, sizeof(constDesc));
		constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constDesc.ByteWidth = size;
		constDesc.Usage = D3D11_USAGE_DEFAULT;
		HRESULT d3dResult = mRenderSystem->getDevice()->CreateBuffer(&constDesc, nullptr, &cbuffer);

		if (FAILED(d3dResult))
		{
			AaLogger::logErrorD3D("failed to create custom cbuffer " + name, d3dResult);
			return;
		}
	}

	mRenderSystem->getContext()->UpdateSubresource(cbuffer, 0, nullptr, data, 0, 0);
}

ID3D11Buffer* AaShaderBuffersManager::getCbuffer(const std::string& name)
{
	auto it = globalCbuffers.find(name);
	return it == globalCbuffers.end() ? nullptr : it->second;
}
