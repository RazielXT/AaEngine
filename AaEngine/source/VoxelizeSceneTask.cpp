#include "VoxelizeSceneTask.h"
#include "SceneManager.h"
#include "AaMaterialResources.h"
#include "AaTextureResources.h"
#include "GenerateMipsComputeShader.h"
#include "DebugWindow.h"

VoxelizeSceneTask::VoxelizeSceneTask(RenderProvider p, SceneManager& s, AaShadowMap& shadows) : shadowMaps(shadows), CompositorTask(p, s)
{
}

VoxelizeSceneTask::~VoxelizeSceneTask()
{
	running = false;

	if (eventBegin)
	{
		SetEvent(eventBegin);
		worker.join();
		commands.deinit();
		CloseHandle(eventBegin);
		CloseHandle(eventFinish);
	}
}

const float VoxelSize = 128;
VoxelSceneSettings settings;

AsyncTasksInfo VoxelizeSceneTask::initialize(CompositorPass& pass)
{
	voxelSceneTexture.create3D(provider.renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT);
	voxelSceneTexture.setName(L"SceneVoxel");
	DescriptorManager::get().createUAVView(voxelSceneTexture);
	AaTextureResources::get().setNamedUAV("SceneVoxel", &voxelSceneTexture.uav.front());
	DescriptorManager::get().createTextureView(voxelSceneTexture);
	AaTextureResources::get().setNamedTexture("SceneVoxel", &voxelSceneTexture.textureView);

	voxelPreviousSceneTexture.create3D(provider.renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT);
	voxelPreviousSceneTexture.setName(L"SceneVoxelBounces");
	DescriptorManager::get().createTextureView(voxelPreviousSceneTexture);
	AaTextureResources::get().setNamedTexture("SceneVoxelBounces", &voxelPreviousSceneTexture.textureView);

	clearSceneTexture.create3D(provider.renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT);
	clearSceneTexture.setName(L"VoxelClear");

	cbuffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");

	computeMips.init(provider.renderSystem->device, "generateMipmaps");

	sceneQueue = sceneMgr.createQueue({ pass.target.texture->format }, MaterialTechnique::Voxelize);

	AsyncTasksInfo tasks;
	eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	commands = provider.renderSystem->CreateCommandList(L"Voxelize");

	worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				auto orthoHalfSize = settings.extends;
				float NearClipDistance = 1;
				camera.setOrthographicCamera(orthoHalfSize.x * 2, orthoHalfSize.y * 2, NearClipDistance, NearClipDistance + orthoHalfSize.z * 2 + 200);

				auto center = settings.center;

				XMFLOAT3 corner(center.x - orthoHalfSize.x, center.y - orthoHalfSize.y, center.z - orthoHalfSize.z);
				updateCBuffer(orthoHalfSize, corner, provider.renderSystem->frameIndex);

				provider.renderSystem->StartCommandList(commands);
				if (imgui::DebugWindow::state.stopUpdatingVoxel)
				{
					SetEvent(eventFinish);
					continue;
				}

				TextureResource::TransitionState(commands.commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
				TextureResource::TransitionState(commands.commandList, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);

				commands.commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), voxelSceneTexture.texture.Get());

				//fast clear
				TextureResource::TransitionState(commands.commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);
				commands.commandList->CopyResource(voxelSceneTexture.texture.Get(), clearSceneTexture.texture.Get());

				commands.commandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

				static RenderObjectsVisibilityData sceneInfo;
				auto& renderables = *sceneMgr.getRenderables(Order::Normal);

				pass.target.texture->PrepareAsTarget(commands.commandList, pass.target.previousState, true);

				TextureResource::TransitionState(commands.commandList, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				TextureResource::TransitionState(commands.commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

				ShaderConstantsProvider constants(provider.params, sceneInfo, camera, *pass.target.texture);

				//from all 3 axes
				//Z
				camera.setPosition(XMFLOAT3(center.x, center.y, center.z - orthoHalfSize.z - NearClipDistance));
				camera.setDirection(XMFLOAT3(0, 0, 1));
				camera.updateMatrix();
				renderables.updateVisibility(camera, sceneInfo);
				sceneQueue->renderObjects(constants, commands.commandList);
				//X
				camera.setPosition(XMFLOAT3(center.x - orthoHalfSize.x - NearClipDistance, center.y, center.z));
				camera.setDirection(XMFLOAT3(1, 0, 0));
				camera.updateMatrix();
				renderables.updateVisibility(camera, sceneInfo);
				sceneQueue->renderObjects(constants, commands.commandList);
				//Y
				camera.setPosition(XMFLOAT3(center.x, center.y - orthoHalfSize.y - NearClipDistance, center.z));
				camera.pitch(XM_PIDIV2);
				camera.updateMatrix();
				renderables.updateVisibility(camera, sceneInfo);
				sceneQueue->renderObjects(constants, commands.commandList);

				computeMips.dispatch(commands.commandList, voxelSceneTexture, DescriptorManager::get());

				SetEvent(eventFinish);
			}
		});

	tasks.emplace_back(eventFinish, commands);

	return tasks;
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CommandsData& syncCommands, CompositorPass&)
{
	SetEvent(eventBegin);
}

void VoxelizeSceneTask::updateCBuffer(Vector3 orthoHalfSize, Vector3 corner, UINT frameIndex)
{
	cbufferData.voxelSceneSize = orthoHalfSize * 2;

	float sceneToVoxel = VoxelSize / (cbufferData.voxelSceneSize.x);
	cbufferData.voxelDensity = sceneToVoxel;

	cbufferData.voxelOffset = corner;

	auto& state = imgui::DebugWindow::state;

	cbufferData.steppingBounces = state.voxelSteppingBounces;
	cbufferData.steppingDiffuse = state.voxelSteppingDiffuse;

	cbufferData.middleConeRatioDistance = state.middleConeRatioDistance;
	cbufferData.sideConeRatioDistance = state.sideConeRatioDistance;

	float deltaTime = provider.params.timeDelta;
	deltaTime += deltaTime - deltaTime * deltaTime;
	deltaTime = max(deltaTime, 1.0f);

	cbufferData.voxelizeLighting = 0.0f;

	cbufferData.lerpFactor = deltaTime;
	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &cbufferData, sizeof(cbufferData));
}

void VoxelizeSceneTask::updateCBuffer(bool lighting, UINT frameIndex)
{
	cbufferData.voxelizeLighting = lighting ? 1.0f : 0.0f;
	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &cbufferData, sizeof(cbufferData));
}

VoxelSceneSettings& VoxelSceneSettings::get()
{
	return settings;
}
