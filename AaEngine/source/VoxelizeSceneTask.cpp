#include "VoxelizeSceneTask.h"
#include "AaSceneManager.h"
#include "AaMaterialResources.h"
#include "AaTextureResources.h"
#include "GenerateMipsComputeShader.h"

VoxelizeSceneTask::VoxelizeSceneTask(AaShadowMap& shadows) : shadowMaps(shadows)
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

extern bool stopUpdatingVoxel;

const float VoxelSize = 128;

AsyncTasksInfo VoxelizeSceneTask::initialize(AaRenderSystem* renderSystem, AaSceneManager* sceneMgr, RenderTargetTexture* target)
{
	voxelSceneTexture.create3D(renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT, renderSystem->FrameCount);
	voxelSceneTexture.setName(L"SceneVoxel");
	ResourcesManager::get().createUAVView(voxelSceneTexture);
	AaTextureResources::get().setNamedUAV("SceneVoxel", &voxelSceneTexture.uav.front());
	ResourcesManager::get().createShaderResourceView(voxelSceneTexture);
	AaTextureResources::get().setNamedTexture("SceneVoxel", &voxelSceneTexture.textureView);

	voxelPreviousSceneTexture.create3D(renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT, renderSystem->FrameCount);
	voxelPreviousSceneTexture.setName(L"SceneVoxelBounces");
	ResourcesManager::get().createShaderResourceView(voxelPreviousSceneTexture);
	AaTextureResources::get().setNamedTexture("SceneVoxelBounces", &voxelPreviousSceneTexture.textureView);

	clearSceneTexture.create3D(renderSystem->device, VoxelSize, VoxelSize, VoxelSize, DXGI_FORMAT_R16G16B16A16_FLOAT, renderSystem->FrameCount);
	clearSceneTexture.setName(L"VoxelClear");

	cbuffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");

	computeMips.init(renderSystem->device, "generateMipmaps");

	sceneQueue = sceneMgr->createQueue(target->formats, MaterialVariant::Voxel);

	AsyncTasksInfo tasks;
	eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	commands = renderSystem->CreateCommandList(L"Voxelize");

	worker = std::thread([this]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				const auto FrameIndex = ctx.renderSystem->frameIndex;

				auto orthoHalfSize = XMFLOAT3(150, 150, 150);
				camera.setOrthograhicCamera(orthoHalfSize.x * 2, orthoHalfSize.y * 2, 1, 1 + orthoHalfSize.z * 2 + 200);

				auto center = XMFLOAT3(0, 0, 0);

				XMFLOAT3 corner(center.x - orthoHalfSize.x, center.y - orthoHalfSize.y, center.z - orthoHalfSize.z);
				updateCBuffer(orthoHalfSize, corner, FrameIndex);

				static int counter = 0;
				bool skipUpdate = counter >= 2;
				counter++;
				if (counter > 2 * 6)
					counter = 0;

				ctx.renderSystem->StartCommandList(commands);
				if (stopUpdatingVoxel)
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

				ctx.target->PrepareAsTarget(commands.commandList, FrameIndex, true, false, false);

				TextureResource::TransitionState(commands.commandList, FrameIndex, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				TextureResource::TransitionState(commands.commandList, FrameIndex, voxelSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

				//from all 3 axes
				camera.setPosition(XMFLOAT3(center.x, center.y, center.z - orthoHalfSize.z - 1));
				camera.lookTo(XMFLOAT3(0, 0, 1));
				camera.updateMatrix();
				ctx.renderables->updateVisibility(camera.prepareOrientedBox(), info.visibility);
				ctx.renderables->updateWVPMatrix(camera.getViewProjectionMatrix(), info.visibility, info.wvpMatrix);
				sceneQueue->renderObjects(camera, info, ctx.params, commands.commandList, FrameIndex);

				camera.setPosition(XMFLOAT3(center.x - orthoHalfSize.x - 1, center.y, center.z));
				camera.lookTo(XMFLOAT3(1, 0, 0));
				camera.updateMatrix();
				ctx.renderables->updateVisibility(camera.prepareOrientedBox(), info.visibility);
				ctx.renderables->updateWVPMatrix(camera.getViewProjectionMatrix(), info.visibility, info.wvpMatrix);
				sceneQueue->renderObjects(camera, info, ctx.params, commands.commandList, FrameIndex);

				camera.setPosition(XMFLOAT3(center.x, center.y - orthoHalfSize.y - 1, center.z));
				camera.pitch(3.14 / 2.0f);
				camera.updateMatrix();
				ctx.renderables->updateVisibility(camera.prepareOrientedBox(), info.visibility);
				ctx.renderables->updateWVPMatrix(camera.getViewProjectionMatrix(), info.visibility, info.wvpMatrix);
				sceneQueue->renderObjects(camera, info, ctx.params, commands.commandList, FrameIndex);

// 				updateCBuffer(false, FrameIndex);
// 
// 				ctx.renderables->updateVisibility(shadowMaps.camera[1].prepareOrientedBox(), info.visibility);
// 				ctx.renderables->updateWVPMatrix(shadowMaps.camera[1].getViewProjectionMatrix(), info.visibility, info.wvpMatrix);
// 				sceneQueue->renderObjects(shadowMaps.camera[1], info, ctx.params, commands.commandList, FrameIndex);

				computeMips.dispatch(commands.commandList, voxelSceneTexture, ResourcesManager::get(), FrameIndex);

				SetEvent(eventFinish);
			}
		});

	tasks.emplace_back(eventFinish, commands);

	return tasks;
}

void VoxelizeSceneTask::prepare(RenderContext& renderCtx, CommandsData& syncCommands)
{
	ctx = renderCtx;
	SetEvent(eventBegin);
}

extern float voxelSteppingBounces;
extern float voxelSteppingDiffuse;
extern Vector2 middleConeRatioDistance;
extern Vector2 sideConeRatioDistance;

void VoxelizeSceneTask::updateCBuffer(Vector3 orthoHalfSize, Vector3 corner, UINT frameIndex)
{
	float sceneToVoxel = VoxelSize / (2 * orthoHalfSize.x);
	cbufferData.voxelDensity = sceneToVoxel;

	cbufferData.voxelOffset = corner;

	cbufferData.steppingBounces = voxelSteppingBounces;
	cbufferData.steppingDiffuse = voxelSteppingDiffuse;

	cbufferData.middleConeRatioDistance = middleConeRatioDistance;
	cbufferData.sideConeRatioDistance = sideConeRatioDistance;

	float deltaTime = ctx.params.timeDelta * 2;
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
