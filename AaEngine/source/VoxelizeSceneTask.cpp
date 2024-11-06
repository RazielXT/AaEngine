#include "VoxelizeSceneTask.h"
#include "AaSceneManager.h"
#include "AaMaterialResources.h"
#include "AaTextureResources.h"
#include "GenerateMipsComputeShader.h"
#include "DebugWindow.h"

VoxelizeSceneTask::VoxelizeSceneTask(RenderProvider p, AaShadowMap& shadows) : shadowMaps(shadows), provider(p)
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

AsyncTasksInfo VoxelizeSceneTask::initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target)
{
	voxelSceneTexture.create3D(provider.renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT, provider.renderSystem->FrameCount);
	voxelSceneTexture.setName(L"SceneVoxel");
	ResourcesManager::get().createUAVView(voxelSceneTexture);
	AaTextureResources::get().setNamedUAV("SceneVoxel", &voxelSceneTexture.uav.front());
	ResourcesManager::get().createShaderResourceView(voxelSceneTexture);
	AaTextureResources::get().setNamedTexture("SceneVoxel", &voxelSceneTexture.textureView);

	voxelPreviousSceneTexture.create3D(provider.renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT, provider.renderSystem->FrameCount);
	voxelPreviousSceneTexture.setName(L"SceneVoxelBounces");
	ResourcesManager::get().createShaderResourceView(voxelPreviousSceneTexture);
	AaTextureResources::get().setNamedTexture("SceneVoxelBounces", &voxelPreviousSceneTexture.textureView);

	clearSceneTexture.create3D(provider.renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT, provider.renderSystem->FrameCount);
	clearSceneTexture.setName(L"VoxelClear");

	cbuffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");

	computeMips.init(provider.renderSystem->device, "generateMipmaps");

	sceneQueue = sceneMgr->createQueue(target->formats, MaterialTechnique::Voxelize);

	AsyncTasksInfo tasks;
	eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	commands = provider.renderSystem->CreateCommandList(L"Voxelize");

	worker = std::thread([this]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				const auto FrameIndex = provider.renderSystem->frameIndex;

				auto orthoHalfSize = XMFLOAT3(150, 150, 150);
				camera.setOrthographicCamera(orthoHalfSize.x * 2, orthoHalfSize.y * 2, 1, 1 + orthoHalfSize.z * 2 + 200);

				auto center = XMFLOAT3(0, 0, 0);

				XMFLOAT3 corner(center.x - orthoHalfSize.x, center.y - orthoHalfSize.y, center.z - orthoHalfSize.z);
				updateCBuffer(orthoHalfSize, corner, FrameIndex);

				static int counter = 0;
				bool skipUpdate = counter >= 2;
				counter++;
				if (counter > 2 * 6)
					counter = 0;

				provider.renderSystem->StartCommandList(commands);
				if (imgui::DebugWindow::state.stopUpdatingVoxel)
				{
					SetEvent(eventFinish);
					continue;
				}

				TextureResource::TransitionState(commands.commandList, FrameIndex, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
				TextureResource::TransitionState(commands.commandList, FrameIndex, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);

				commands.commandList->CopyResource(voxelPreviousSceneTexture.textures[FrameIndex].Get(), voxelSceneTexture.textures[FrameIndex].Get());

				//fast clear
				TextureResource::TransitionState(commands.commandList, FrameIndex, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);
				commands.commandList->CopyResource(voxelSceneTexture.textures[FrameIndex].Get(), clearSceneTexture.textures[FrameIndex].Get());

				commands.commandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

				static RenderInformation info;

				//ctx.target->PrepareAsTarget(commands.commandList, FrameIndex, D3D12_RESOURCE_STATE_COMMON, true, false, false);

				TextureResource::TransitionState(commands.commandList, FrameIndex, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				TextureResource::TransitionState(commands.commandList, FrameIndex, voxelSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

				//from all 3 axes
				camera.setPosition(XMFLOAT3(center.x, center.y, center.z - orthoHalfSize.z - 1));
				camera.setDirection(XMFLOAT3(0, 0, 1));
				ctx.renderables->updateRenderInformation(camera, info);
				sceneQueue->renderObjects(camera, info, provider.params, commands.commandList, FrameIndex);

				camera.setPosition(XMFLOAT3(center.x - orthoHalfSize.x - 1, center.y, center.z));
				camera.setDirection(XMFLOAT3(1, 0, 0));
				ctx.renderables->updateRenderInformation(camera, info);
				sceneQueue->renderObjects(camera, info, provider.params, commands.commandList, FrameIndex);

				camera.setPosition(XMFLOAT3(center.x, center.y - orthoHalfSize.y - 1, center.z));
				camera.pitch(XM_PIDIV2);
				ctx.renderables->updateRenderInformation(camera, info);
				sceneQueue->renderObjects(camera, info, provider.params, commands.commandList, FrameIndex);

				computeMips.dispatch(commands.commandList, voxelSceneTexture, ResourcesManager::get(), FrameIndex);

				SetEvent(eventFinish);
			}
		});

	tasks.emplace_back(eventFinish, commands);

	return tasks;
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CommandsData& syncCommands)
{
	ctx = renderCtx;
	SetEvent(eventBegin);
}

void VoxelizeSceneTask::updateCBuffer(Vector3 orthoHalfSize, Vector3 corner, UINT frameIndex)
{
	float sceneToVoxel = VoxelSize / (2 * orthoHalfSize.x);
	cbufferData.voxelDensity = sceneToVoxel;

	cbufferData.voxelOffset = corner;

	auto& state = imgui::DebugWindow::state;

	cbufferData.steppingBounces = state.voxelSteppingBounces;
	cbufferData.steppingDiffuse = state.voxelSteppingDiffuse;

	cbufferData.middleConeRatioDistance = state.middleConeRatioDistance;
	cbufferData.sideConeRatioDistance = state.sideConeRatioDistance;

	float deltaTime = provider.params.timeDelta * 2;
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
