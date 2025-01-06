#include "ShadowMap.h"
#include "GraphicsResources.h"
#include "directx\d3dx12.h"
#include <format>
#include "CascadedShadowMaps.h"

AaShadowMap::AaShadowMap(SceneLights::Light& l,	PssmParameters& d) : sun(l), data(d)
{
}

void AaShadowMap::init(RenderSystem& renderSystem, GraphicsResources& resources)
{
	const UINT count = std::size(texture);
	targetHeap.InitDsv(renderSystem.core.device, count, L"ShadowMapsDSV");

	for (UINT i = 0; i < count; i++)
	{
		texture[i].DepthClearValue = 1.0f;
		texture[i].InitDepth(renderSystem.core.device, 512, 512, targetHeap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		texture[i].SetName("ShadowMap");

		DescriptorManager::get().createTextureView(texture[i]);
		resources.textures.setNamedTexture("ShadowMap" + std::to_string(i), texture[i].view);

		camera[i].setOrthographicCamera(400 + i * 400, 400 + i * 400, 1, 1000);
	}

	data.TexIdShadowOffsetStart = texture[0].view.srvHeapIndex;
	data.ShadowMapSize = 512;
	data.ShadowMapSizeInv = 1 / data.ShadowMapSize;

	cbuffer = resources.shaderBuffers.CreateCbufferResource(sizeof(data), "PSSMShadows");

	update(renderSystem.core.frameIndex, {});
}

static const XMVECTORF32 s_lightEye = { 300.0f, 300.0f, 300.0f, 0.f };

void AaShadowMap::update(UINT frameIndex, Camera& mainCamera)
{
	for (int i = 0; auto& c : camera)
	{
		c.lookTo(s_lightEye, g_XMZero);

		ShadowMapCascade cascade;
		cascade.update(c, mainCamera);

		c.setProjection(cascade.m_matShadowProj[i++]);
	}

	{
		auto& cTest = camera[1];
		float extends = 600;

		FLOAT fWorldUnitsPerTexel = extends * 2 / (float)512.f;
		auto vWorldUnitsPerTexel = XMVectorSet(fWorldUnitsPerTexel, fWorldUnitsPerTexel, 0.0f, 0.0f);

		XMVECTOR myPos = mainCamera.getPosition();
		auto myPosLightSpace = XMVector4Transform(myPos, cTest.getViewMatrix());

		XMVECTOR vLightCameraOrthographicMin = { -extends, -extends };
		XMVECTOR vLightCameraOrthographicMax = { extends, extends };

		vLightCameraOrthographicMin = XMVectorAdd(vLightCameraOrthographicMin, myPosLightSpace);
		vLightCameraOrthographicMax = XMVectorAdd(vLightCameraOrthographicMax, myPosLightSpace);

		{
			// We snape the camera to 1 pixel increments so that moving the camera does not cause the shadows to jitter.
			// This is a matter of integer dividing by the world space size of a texel
			vLightCameraOrthographicMin /= vWorldUnitsPerTexel;
			vLightCameraOrthographicMin = XMVectorFloor(vLightCameraOrthographicMin);
			vLightCameraOrthographicMin *= vWorldUnitsPerTexel;

			vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
			vLightCameraOrthographicMax = XMVectorFloor(vLightCameraOrthographicMax);
			vLightCameraOrthographicMax *= vWorldUnitsPerTexel;
		}

		auto m_matShadowProj = XMMatrixOrthographicOffCenterLH(XMVectorGetX(vLightCameraOrthographicMin), XMVectorGetX(vLightCameraOrthographicMax),
			XMVectorGetY(vLightCameraOrthographicMin), XMVectorGetY(vLightCameraOrthographicMax),
			0, 1000);

		cTest.setProjection(m_matShadowProj);
	}

	XMStoreFloat4x4(&data.ShadowMatrix[0], XMMatrixTranspose(camera[0].getViewProjectionMatrix()));
	XMStoreFloat4x4(&data.ShadowMatrix[1], XMMatrixTranspose(camera[1].getViewProjectionMatrix()));
	data.SunDirection = sun.direction;
	data.SunColor = sun.color;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &data, sizeof(data));
}

static Vector3 SnapToGrid(Vector3 position, float gridStep)
{
	position.x = round(position.x / gridStep) * gridStep;
	position.y = round(position.y / gridStep) * gridStep;
	position.z = round(position.z / gridStep) * gridStep;
	return position;
}

void AaShadowMap::update(UINT frameIndex, Vector3 center)
{
// 	Vector3 posCenter{};
// 	{
// 		float worldTexelSize = 400 /512.f;
// 
// 		//get texCam orientation
// 		Vector3 dir = sun.direction;
// 
// 		Vector3 up = Vector3::UnitY;
// 		// Check it's not coincident with dir
// 		if (abs(up.Dot(dir)) >= 1.0f)
// 		{
// 			// Use camera up
// 			up = Vector3::UnitZ;
// 		}
// 		// cross twice to rederive, only direction is unaltered
// 		Vector3 left = dir.Cross(up);
// 		left.Normalize();
// 		up = dir.Cross(left);
// 		up.Normalize();
// 		// Derive quaternion from axes
// 		Quaternion q = Quaternion::LookRotation(dir, up);
// 		//q.CreateFromAxisAngle(left, up, dir);
// 
// 		//convert world space camera position into light space
// 		Quaternion invQ;
// 		q.Inverse(invQ);
// 		Vector3 lightSpacePos = invQ * center;
// 
// 		//snap to nearest texel
// 		//lightSpacePos.x=Math::Floor(lightSpacePos.x*worldTexelSize+0.5)/worldTexelSize;
// 		//lightSpacePos.y=Math::Floor(lightSpacePos.y*worldTexelSize+0.5)/worldTexelSize;
// 		lightSpacePos.x -= fmod(lightSpacePos.x, worldTexelSize);
// 		lightSpacePos.y -= fmod(lightSpacePos.y, worldTexelSize);
// 
// 		//convert back to world space
// 		posCenter = q * lightSpacePos;
// 
// 		std::string logg = std::format("{} {} {} \n", posCenter.x, posCenter.y, posCenter.z);
// 		OutputDebugStringA(logg.c_str());
// 	}
	//center = SnapToGrid(center, 512 / 400.f);

	for (auto& c : camera)
	{
		c.setDirection(sun.direction);
		c.setPosition(center + XMFLOAT3{ sun.direction.x * -300, sun.direction.y * -300, sun.direction.z * -300 });
		c.updateMatrix();
	}

	XMStoreFloat4x4(&data.ShadowMatrix[0], XMMatrixTranspose(camera[0].getViewProjectionMatrix()));
	XMStoreFloat4x4(&data.ShadowMatrix[1], XMMatrixTranspose(camera[1].getViewProjectionMatrix()));
	data.SunDirection = sun.direction;
	data.SunColor = sun.color;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &data, sizeof(data));
}

void AaShadowMap::clear(ID3D12GraphicsCommandList* commandList)
{
	for (auto& t : texture)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			t.texture.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->ResourceBarrier(1, &barrier);

		t.ClearDepth(commandList);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			t.texture.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
	}
}
