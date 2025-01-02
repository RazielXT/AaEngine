#include "VoxelizeSceneTask.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include "TextureResources.h"
#include "GenerateMipsComputeShader.h"
#include "DebugWindow.h"
#include <format>

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

Vector3 SnapToGrid(Vector3 position, float gridStep)
{
	position.x = round(position.x / gridStep) * gridStep;
	position.y = round(position.y / gridStep) * gridStep;
	position.z = round(position.z / gridStep) * gridStep;
	return position;
}

AsyncTasksInfo VoxelizeSceneTask::initialize(CompositorPass& pass)
{
	voxelSceneTexture.create3D(provider.renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R8G8B8A8_UNORM); //DXGI_FORMAT_R16G16B16A16_FLOAT
	voxelSceneTexture.setName("SceneVoxel");
	provider.resources.descriptors.createUAVView(voxelSceneTexture);
	provider.resources.textures.setNamedUAV("SceneVoxel", voxelSceneTexture.uav.front());
	provider.resources.descriptors.createTextureView(voxelSceneTexture);
	provider.resources.textures.setNamedTexture("SceneVoxel", voxelSceneTexture.textureView);

	voxelPreviousSceneTexture.create3D(provider.renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R8G8B8A8_UNORM);
	voxelPreviousSceneTexture.setName("SceneVoxelBounces");
	provider.resources.descriptors.createTextureView(voxelPreviousSceneTexture);
	provider.resources.textures.setNamedTexture("SceneVoxelBounces", voxelPreviousSceneTexture.textureView);

	clearSceneTexture.create3D(provider.renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R8G8B8A8_UNORM);
	clearSceneTexture.setName("VoxelClear");

	cbuffer = provider.resources.shaderBuffers.CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");

	computeMips.init(*provider.renderSystem.core.device, "generateMipmaps", provider.resources.shaders);

	sceneQueue = sceneMgr.createQueue({ pass.target.texture->format }, MaterialTechnique::Voxelize);

	AsyncTasksInfo tasks;
	eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	commands = provider.renderSystem.core.CreateCommandList(L"Voxelize");

	worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				auto orthoHalfSize = settings.extends;
				float NearClipDistance = 1;
				camera.setOrthographicCamera(orthoHalfSize.x * 2, orthoHalfSize.y * 2, NearClipDistance, NearClipDistance + orthoHalfSize.z * 2 + 200);

				auto center = settings.center;
				float step = orthoHalfSize.x * 2 / 128.f;
				//step *= 4;

				center = SnapToGrid(ctx.camera->getPosition(), step);

				static auto lastCenter = center;

				auto diff = (lastCenter - center) / step;
				lastCenter = center;
				std::string logg = std::format("diff {} {} {} \n", diff.x, diff.y, diff.z);
				OutputDebugStringA(logg.c_str());

				XMFLOAT3 corner(center.x - orthoHalfSize.x, center.y - orthoHalfSize.y, center.z - orthoHalfSize.z);
				updateCBuffer(orthoHalfSize, corner, diff, provider.renderSystem.core.frameIndex);

				auto marker = provider.renderSystem.core.StartCommandList(commands);
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
				TextureResource::TransitionState(commands.commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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
				//renderables.updateVisibility(camera, sceneInfo);
				sceneQueue->renderObjects(constants, commands.commandList);
				//Y
				camera.setPosition(XMFLOAT3(center.x, center.y - orthoHalfSize.y - NearClipDistance, center.z));
				camera.pitch(XM_PIDIV2);
				camera.updateMatrix();
				//renderables.updateVisibility(camera, sceneInfo);
				sceneQueue->renderObjects(constants, commands.commandList);

				marker.move("VoxelMipMaps");

				computeMips.dispatch(commands.commandList, voxelSceneTexture);

				marker.close();
				SetEvent(eventFinish);
			}
		});

	tasks.emplace_back(eventFinish, commands);

	return tasks;
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CommandsData& syncCommands, CompositorPass&)
{
	ctx = renderCtx;
	SetEvent(eventBegin);
}

void VoxelizeSceneTask::updateCBuffer(Vector3 orthoHalfSize, Vector3 corner, Vector3 diff, UINT frameIndex)
{
	cbufferData.voxelSceneSize = orthoHalfSize * 2;

	float sceneToVoxel = VoxelSize / (cbufferData.voxelSceneSize.x);
	cbufferData.voxelDensity = sceneToVoxel;

	cbufferData.voxelOffset = corner;

	auto& state = imgui::DebugWindow::state;

	cbufferData.steppingBounces = state.voxelSteppingBounces;
	cbufferData.steppingDiffuse = state.voxelSteppingDiffuse;
	cbufferData.diff = diff;

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

VoxelSceneSettings& VoxelSceneSettings::get()
{
	return settings;
}
