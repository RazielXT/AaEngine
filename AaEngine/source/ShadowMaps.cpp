#include "ShadowMaps.h"
#include "GraphicsResources.h"
#include "directx\d3dx12.h"
#include <format>

const UINT shadowMapSize = 1024;

ShadowMaps::ShadowMaps(SceneLights::Light& l,	PssmParameters& d) : sun(l), data(d)
{
	for (int i = 0; auto c : cascadeInfo.cascadePartitionsZeroToOne)
	{
		data.ShadowCascadeDistance[i++] = (float)c;
	}	
}

void ShadowMaps::init(RenderSystem& renderSystem, GraphicsResources& resources)
{
	const UINT count = std::size(cascades);
	targetHeap.InitDsv(renderSystem.core.device, count + 1, L"ShadowMapsDSV");

	for (UINT i = 0; i < count; i++)
	{
		auto name = "ShadowMapCascade" + std::to_string(i);

		cascades[i].texture.DepthClearValue = 1.0f;
		cascades[i].texture.InitDepth(renderSystem.core.device, shadowMapSize, shadowMapSize, targetHeap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cascades[i].texture.SetName(name);

		DescriptorManager::get().createTextureView(cascades[i].texture);
		//resources.textures.setNamedTexture(name, cascades[i].texture.view);
	}

	{
		maxShadow.texture.DepthClearValue = 1.0f;
		maxShadow.texture.InitDepth(renderSystem.core.device, shadowMapSize, shadowMapSize, targetHeap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		maxShadow.texture.SetName("ShadowMapMax");

		DescriptorManager::get().createTextureView(maxShadow.texture);
	}

	data.TexIdShadowOffsetStart = cascades[0].texture.view.srvHeapIndex;
	data.ShadowMapSize = shadowMapSize;
	data.ShadowMapSizeInv = 1 / data.ShadowMapSize;



	cbuffer = resources.shaderBuffers.CreateCbufferResource(sizeof(data), "PSSMShadows");

	Camera tmp;
	tmp.setPerspectiveCamera(60, 1, 1, 100);
	update(renderSystem.core.frameIndex, tmp);
}

#include <format>

void ShadowMaps::update(UINT frameIndex, Camera& mainCamera)
{
	const float LightRange = 5000.f;

	const XMVECTORF32 lightEye = { sun.direction.x * -LightRange, sun.direction.y * -LightRange, sun.direction.z * -LightRange, 0.f };
	const XMVECTORF32 lightTarget = g_XMZero;

	cascades[0].camera.lookTo(lightEye, lightTarget);

	Vector2 nearFarPlane{};
	cascadeInfo.update(cascades[0].camera, mainCamera, LightRange, nearFarPlane, shadowMapSize);

	for (int i = 0; auto& cascade : cascades)
	{
// 		if (i == 2 && counter % 2 != 0)
// 		{
// 			cascade.update = false;
// 			continue;
// 		}

		cascade.camera.lookTo(lightEye, lightTarget);
		cascade.camera.setOrthographicProjection(cascadeInfo.matShadowProj[i++]);
		cascade.update = true;
	}

// 	if (counter != 0)
// 	{
// 		maxShadow.update = false;
// 	}
// 	else
	{
		maxShadow.camera.lookTo(lightEye, lightTarget);

		float extends = 6000;

		float fWorldUnitsPerTexel = extends * 2 / shadowMapSize;
		auto vWorldUnitsPerTexel = XMVectorSet(fWorldUnitsPerTexel, fWorldUnitsPerTexel, 0.0f, 0.0f);

		XMVECTOR myPos = mainCamera.getPosition();
		auto myPosLightSpace = XMVector4Transform(myPos, maxShadow.camera.getViewMatrix());

		XMVECTOR vLightCameraOrthographicMin = { -extends, -extends };
		XMVECTOR vLightCameraOrthographicMax = { extends, extends };

		vLightCameraOrthographicMin = XMVectorAdd(vLightCameraOrthographicMin, myPosLightSpace);
		vLightCameraOrthographicMax = XMVectorAdd(vLightCameraOrthographicMax, myPosLightSpace);

		{
			// We snap the camera to 1 pixel increments so that moving the camera does not cause the shadows to jitter.
			// This is a matter of integer dividing by the world space size of a texel
			vLightCameraOrthographicMin /= vWorldUnitsPerTexel;
			vLightCameraOrthographicMin = XMVectorFloor(vLightCameraOrthographicMin);
			vLightCameraOrthographicMin *= vWorldUnitsPerTexel;

			vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
			vLightCameraOrthographicMax = XMVectorFloor(vLightCameraOrthographicMax);
			vLightCameraOrthographicMax *= vWorldUnitsPerTexel;
		}

		auto matShadowProj = XMMatrixOrthographicOffCenterLH(XMVectorGetX(vLightCameraOrthographicMin), XMVectorGetX(vLightCameraOrthographicMax),
			XMVectorGetY(vLightCameraOrthographicMin), XMVectorGetY(vLightCameraOrthographicMax),
			nearFarPlane.x, nearFarPlane.y);

		maxShadow.camera.setOrthographicProjection(matShadowProj);
		maxShadow.update = true;
	}

	for (size_t i = 0; i < _countof(cascades); i++)
	{
		XMStoreFloat4x4(&data.ShadowMatrix[i], XMMatrixTranspose(cascades[i].camera.getViewProjectionMatrix()));
	}
	XMStoreFloat4x4(&data.MaxShadowMatrix, XMMatrixTranspose(maxShadow.camera.getViewProjectionMatrix()));
	data.ShadowMatrix[3] = data.MaxShadowMatrix;

	data.SunDirection = sun.direction;
	data.SunColor = sun.color;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &data, sizeof(data));

	counter++;
}

static Vector3 SnapToGrid(Vector3 position, float gridStep)
{
	position.x = round(position.x / gridStep) * gridStep;
	position.y = round(position.y / gridStep) * gridStep;
	position.z = round(position.z / gridStep) * gridStep;
	return position;
}

void ShadowMaps::clear(ID3D12GraphicsCommandList* commandList)
{
	CD3DX12_RESOURCE_BARRIER barriers[_countof(cascades) + 1];
	for (int i = 0; auto& c : cascades)
	{
		barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(
			c.texture.texture.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	barriers[_countof(cascades)] = CD3DX12_RESOURCE_BARRIER::Transition(
		maxShadow.texture.texture.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);
	commandList->ResourceBarrier(_countof(barriers), barriers);

	for (auto& c : cascades)
		c.texture.ClearDepth(commandList);
	maxShadow.texture.ClearDepth(commandList);

	for (int i = 0; auto& c : cascades)
	{
		barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(
			c.texture.texture.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	barriers[_countof(cascades)] = CD3DX12_RESOURCE_BARRIER::Transition(
		maxShadow.texture.texture.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(_countof(barriers), barriers);

	counter = 0;
}
