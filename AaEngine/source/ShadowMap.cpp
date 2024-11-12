#include "ShadowMap.h"
#include "ResourcesManager.h"
#include "../Src/d3dx12.h"

AaShadowMap::AaShadowMap(aa::SceneLights::Light& l) : sun(l)
{
}

void AaShadowMap::init(AaRenderSystem* renderSystem)
{
	for (UINT i = 0; i < 2; i++)
	{
		texture[i].Init(renderSystem->device, 512, 512, renderSystem->FrameCount, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		texture[i].SetName(L"ShadowMap");

		ResourcesManager::get().createDepthShaderResourceView(texture[i]);
		AaTextureResources::get().setNamedTexture("ShadowMap" + std::to_string(i), &texture[i].depthView);

		camera[i].setOrthographicCamera(400 + i * 400, 400 + i * 400, 1, 1000);
	}

	data.TexIdShadowOffsetStart = texture[0].depthView.srvHeapIndex;
	data.ShadowMapSize = 512;
	data.ShadowMapSizeInv = 1 / data.ShadowMapSize;

	cbuffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(data), "PSSMShadows");

	update(renderSystem->frameIndex);
}

void AaShadowMap::update(UINT frameIndex)
{
	for (auto& c : camera)
	{
		c.setDirection(sun.direction);
		c.setPosition(XMFLOAT3{ sun.direction.x * -300 + 150, sun.direction.y * -300, sun.direction.z * -300 - 50 });
		c.updateMatrix();
	}

	XMStoreFloat4x4(&data.ShadowMatrix[0], XMMatrixTranspose(camera[0].getViewProjectionMatrix()));
	XMStoreFloat4x4(&data.ShadowMatrix[1], XMMatrixTranspose(camera[1].getViewProjectionMatrix()));
	data.SunDirection = sun.direction;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &data, sizeof(data));
}

void AaShadowMap::clear(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	for (auto& t : texture)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			t.depthStencilTexture[frameIndex].Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->ResourceBarrier(1, &barrier);

		t.Clear(commandList, frameIndex);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			t.depthStencilTexture[frameIndex].Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
	}
}
