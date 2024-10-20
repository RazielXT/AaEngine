#include "VoxelizeSceneTask.h"
#include "AaSceneManager.h"
#include "AaMaterialResources.h"
#include "AaTextureResources.h"
#include "ComputeShader.h"

VoxelizeSceneTask::VoxelizeSceneTask()
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

XM_ALIGNED_STRUCT(16) SceneVoxelInfo
{
	Vector3 voxelOffset;
	float voxelSize;
	Vector2 middleCone = { 1, 0.9 };
	Vector2 sideCone = { 2, 0.8 };
	float radius = 2;
};

AsyncTasksInfo VoxelizeSceneTask::initialize(AaRenderSystem* renderSystem, AaSceneManager* sceneMgr, RenderTargetTexture* target)
{
	voxelSceneTexture.create3D(renderSystem->device, 128, 128, 128, DXGI_FORMAT_R16G16B16A16_FLOAT, renderSystem->FrameCount);
	clearSceneTexture.create3D(renderSystem->device, 128, 128, 128, DXGI_FORMAT_R16G16B16A16_FLOAT, renderSystem->FrameCount);
	AaMaterialResources::get().PrepareUAVView(voxelSceneTexture);
	AaTextureResources::get().setNamedUAV("SceneVoxel", &voxelSceneTexture.uav);
	AaMaterialResources::get().PrepareShaderResourceView(voxelSceneTexture);
	AaTextureResources::get().setNamedTexture("SceneVoxel", &voxelSceneTexture.textureView);

	cbuffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(SceneVoxelInfo), "SceneVoxelInfo");

	ComputeShader computeMips;
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

				float size = 128;
				auto orthoHalfSize = XMFLOAT3(30, 30, 30);
				camera.setOrthograhicCamera(orthoHalfSize.x * 2, orthoHalfSize.y * 2, 1, 1 + orthoHalfSize.z * 2 + 200);

				auto offset = XMFLOAT3(0, 0, 0);

				ctx.renderSystem->StartCommandList(commands);
				//fast clear
				commands.commandList->CopyResource(voxelSceneTexture.textures[FrameIndex].Get(), clearSceneTexture.textures[FrameIndex].Get());
				commands.commandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

				XMFLOAT3 corner(offset.x - orthoHalfSize.x, offset.y - orthoHalfSize.y, offset.z - orthoHalfSize.z);
				float sceneToVoxel = size / (2 * orthoHalfSize.x);
				updateCBuffer(corner, FrameIndex);

				thread_local RenderInformation info;

				ctx.target->PrepareAsTarget(commands.commandList, FrameIndex, true, false, false);

				//from all 3 axes
				camera.setPosition(XMFLOAT3(offset.x, offset.y, offset.z - orthoHalfSize.z - 1));
				camera.lookTo(XMFLOAT3(0, 0, 1));
				camera.updateMatrix();
				ctx.renderables->updateVisibility(camera.prepareOrientedBox(), info.visibility);
				ctx.renderables->updateWVPMatrix(camera.getViewProjectionMatrix(), info.visibility, info.wvpMatrix);
				sceneQueue->renderObjects(camera, info, ctx.params, commands.commandList, FrameIndex);

				camera.setPosition(XMFLOAT3(offset.x - orthoHalfSize.x - 1, offset.y, offset.z));
				camera.lookTo(XMFLOAT3(1, 0, 0));
				camera.updateMatrix();
				ctx.renderables->updateVisibility(camera.prepareOrientedBox(), info.visibility);
				ctx.renderables->updateWVPMatrix(camera.getViewProjectionMatrix(), info.visibility, info.wvpMatrix);
				sceneQueue->renderObjects(camera, info, ctx.params, commands.commandList, FrameIndex);

				camera.setPosition(XMFLOAT3(offset.x, offset.y - orthoHalfSize.y - 1, offset.z));
				camera.pitch(3.14 / 2.0f);
				camera.updateMatrix();
				ctx.renderables->updateVisibility(camera.prepareOrientedBox(), info.visibility);
				ctx.renderables->updateWVPMatrix(camera.getViewProjectionMatrix(), info.visibility, info.wvpMatrix);
				sceneQueue->renderObjects(camera, info, ctx.params, commands.commandList, FrameIndex);

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

void VoxelizeSceneTask::updateCBuffer(Vector3 offset, UINT frameIndex)
{
	SceneVoxelInfo cbufferData;

	float size = 128;
	auto orthoHalfSize = XMFLOAT3(30, 30, 30);
	float sceneToVoxel = size / (2 * orthoHalfSize.x);
	cbufferData.voxelSize = sceneToVoxel;

	cbufferData.voxelOffset = offset;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &cbufferData, sizeof(cbufferData));
}
