#include "ShadowMaps.h"
#include "GraphicsResources.h"
#include "directx\d3dx12.h"
#include <format>

namespace
{
	const UINT ShadowMapSize = 1024;
	const float LightRange = 8000.f;
	Vector2 NearFarPlane{};
}

ShadowMaps::ShadowMaps(RenderSystem& rs, SceneLights::Light& l,	PssmParameters& d) : renderSystem(rs), sun(l), data(d)
{
}

void ShadowMaps::init(GraphicsResources& resources)
{
	const UINT count = std::size(cascades);
	targetHeap.InitDsv(renderSystem.core.device, count + 10, L"ShadowMapsDSV");

	for (int i = 0; auto& cascade : cascades)
		createShadowMap(cascade, ShadowMapSize, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, "ShadowMapCascade" + std::to_string(i++));

	for (int i = 0; auto c : cascadeInfo.cascadePartitionsZeroToOne)
		data.ShadowCascadeDistance[i++] = (float)c;

	data.TexIdShadowMap0 = cascades[0].texture.view.srvHeapIndex;
	data.ShadowMapSize = ShadowMapSize;
	data.ShadowMapSizeInv = 1 / data.ShadowMapSize;

	{
		ResourceUploadBatch batch(renderSystem.core.device);
		batch.Begin();

		auto texture = resources.textures.loadFile(*renderSystem.core.device, batch, "SunZenith_Gradient.png");
		data.TexIdSunZenith = resources.descriptors.createTextureView(*texture);
		texture = resources.textures.loadFile(*renderSystem.core.device, batch, "ViewZenith_Gradient.png");
		data.TexIdViewZenith = resources.descriptors.createTextureView(*texture);
		texture = resources.textures.loadFile(*renderSystem.core.device, batch, "SunView_Gradient.png");
		data.TexIdSunView = resources.descriptors.createTextureView(*texture);

		auto uploadResourcesFinished = batch.End(renderSystem.core.commandQueue);
		uploadResourcesFinished.wait();
	}

	cbuffer = resources.shaderBuffers.CreateCbufferResource(sizeof(data), "PSSMShadows");

	Camera tmp;
	tmp.setPerspectiveCamera(60, 1, 1, 100);
	update(renderSystem.core.frameIndex, tmp);
}

void ShadowMaps::update(UINT frameIndex, Camera& mainCamera)
{
	const XMVECTORF32 lightEye = { sun.direction.x * -LightRange, sun.direction.y * -LightRange, sun.direction.z * -LightRange, 0.f };
	const XMVECTORF32 lightTarget = g_XMZero;

	cascades[0].camera.lookTo(lightEye, lightTarget);
	cascadeInfo.update(cascades[0].camera, mainCamera, LightRange, NearFarPlane, ShadowMapSize);

	for (int i = 0; auto& proj : cascadeInfo.matShadowProj)
	{
		auto& cascade = cascades[i++];
		cascade.camera.lookTo(lightEye, lightTarget);
		cascade.camera.setOrthographicProjection(proj);
		cascade.update = true;
	}

	for (size_t i = 0; i < _countof(cascades); i++)
	{
		XMStoreFloat4x4(&data.ShadowMatrix[i], XMMatrixTranspose(cascades[i].camera.getViewProjectionMatrix()));
	}

	data.SunDirection = sun.direction;
	data.SunColor = Vector3::Lerp(Vector3(1,0.3,0.1), sun.color, min(1, abs(sun.direction.y * 1.4f)));

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

void ShadowMaps::clear(ID3D12GraphicsCommandList* commandList)
{
	TextureTransitions<_countof(cascades)> tr;
	for (int i = 0; auto& c : cascades)
		tr.add(c.texture.texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	tr.push(commandList);

	for (auto& c : cascades)
		c.texture.ClearDepth(commandList);

	for (int i = 0; auto& c : cascades)
		tr.add(c.texture.texture.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.push(commandList);
}

void ShadowMaps::createShadowMap(ShadowMap& map, UINT size, D3D12_RESOURCE_STATES state, const std::string& name)
{
	map.texture.InitDepth(renderSystem.core.device, size, size, targetHeap, state, 1.f);
	map.texture.SetName(name);

	DescriptorManager::get().createTextureView(map.texture);
}

void ShadowMaps::removeShadowMap(ShadowMap& map)
{
	targetHeap.RemoveDepthTargetHandle(map.texture.view);
}

void ShadowMaps::updateShadowMap(ShadowMap& map, Vector3 position, float extends) const
{
	const XMVECTORF32 lightEye = { sun.direction.x * -LightRange, sun.direction.y * -LightRange, sun.direction.z * -LightRange, 0.f };
	const XMVECTORF32 lightTarget = g_XMZero;

	map.camera.lookTo(lightEye, lightTarget);

	float fWorldUnitsPerTexel = extends * 2 / map.texture.width;
	auto vWorldUnitsPerTexel = XMVectorSet(fWorldUnitsPerTexel, fWorldUnitsPerTexel, 0.0f, 0.0f);
	auto myPosLightSpace = XMVector4Transform(position, map.camera.getViewMatrix());

	XMVECTOR vLightCameraOrthographicMin = { -extends, -extends };
	XMVECTOR vLightCameraOrthographicMax = { extends, extends };

	vLightCameraOrthographicMin = XMVectorAdd(vLightCameraOrthographicMin, myPosLightSpace);
	vLightCameraOrthographicMax = XMVectorAdd(vLightCameraOrthographicMax, myPosLightSpace);

	{
		vLightCameraOrthographicMin /= vWorldUnitsPerTexel;
		vLightCameraOrthographicMin = XMVectorFloor(vLightCameraOrthographicMin);
		vLightCameraOrthographicMin *= vWorldUnitsPerTexel;

		vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
		vLightCameraOrthographicMax = XMVectorFloor(vLightCameraOrthographicMax);
		vLightCameraOrthographicMax *= vWorldUnitsPerTexel;
	}

	auto matShadowProj = XMMatrixOrthographicOffCenterLH(XMVectorGetX(vLightCameraOrthographicMin), XMVectorGetX(vLightCameraOrthographicMax),
		XMVectorGetY(vLightCameraOrthographicMin), XMVectorGetY(vLightCameraOrthographicMax),
		NearFarPlane.x, NearFarPlane.y);

	map.camera.setOrthographicProjection(matShadowProj);
}
