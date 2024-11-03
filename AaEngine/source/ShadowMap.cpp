#include "ShadowMap.h"
#include "ResourcesManager.h"

AaShadowMap::AaShadowMap(aa::SceneLights::Light& l) : light(l)
{
}

struct
{
	XMFLOAT4X4 ShadowMatrix[2];
	XMFLOAT3 LightDirection;
	UINT TexIdShadowOffsetStart;
}
data;

void AaShadowMap::init(AaRenderSystem* renderSystem)
{
	for (UINT i = 0; i < 2; i++)
	{
		texture[i].Init(renderSystem->device, 512, 512, renderSystem->FrameCount);
		texture[i].SetName(L"ShadowMap");

		ResourcesManager::get().createDepthShaderResourceView(texture[i]);
		AaTextureResources::get().setNamedTexture("ShadowMap" + std::to_string(i), &texture[i].depthView);

		camera[i].setOrthographicCamera(400 + i * 400, 400 + i * 400, 1, 1000);
	}

	data.TexIdShadowOffsetStart = texture[0].depthView.srvHeapIndex;

	cbuffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(data), "PSSMShadows");

	update(renderSystem->frameIndex);
}

void AaShadowMap::update(UINT frameIndex)
{
	for (auto& c : camera)
	{
		c.setDirection(light.direction);
		c.setPosition(XMFLOAT3{ light.direction.x * -300, light.direction.y * -300, light.direction.z * -300 });
		c.updateMatrix();
	}

	XMStoreFloat4x4(&data.ShadowMatrix[0], XMMatrixTranspose(camera[0].getViewProjectionMatrix()));
	XMStoreFloat4x4(&data.ShadowMatrix[1], XMMatrixTranspose(camera[1].getViewProjectionMatrix()));
	data.LightDirection = light.direction;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &data, sizeof(data));
}
