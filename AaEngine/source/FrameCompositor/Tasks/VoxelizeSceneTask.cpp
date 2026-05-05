#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "Scene/RenderWorld.h"

static VoxelizeSceneTask* instance = nullptr;

VoxelizeSceneTask::VoxelizeSceneTask(RenderProvider p, RenderWorld& w, ShadowMaps& shadows) : shadowMaps(shadows), CompositorTask(p, w)
{
	instance = this;
}

VoxelizeSceneTask::~VoxelizeSceneTask()
{
	running = false;

	voxelization.shutdown();

	if (eventBegin)
	{
		SetEvent(eventBegin);
		worker.join();
		voxelizeCommands.deinit();
		CloseHandle(eventBegin);
		CloseHandle(eventFinish);
	}

	instance = nullptr;
}

VoxelizeSceneTask& VoxelizeSceneTask::Get()
{
	return *instance;
}

void VoxelizeSceneTask::clear(ID3D12GraphicsCommandList* list)
{
	voxelization.clear(list ? list : voxelizeCommands.commandList);
}

CompositorTask::RunType VoxelizeSceneTask::getRunType(CompositorPass& pass) const
{
	if (pass.info.entry == "EndFrame")
		return RunType::SyncCommands;
	if (pass.info.entry == "Bounces")
		return RunType::SyncComputeCommands;

	return RunType::Generic;
}

void VoxelizeSceneTask::revoxelize()
{
	voxelization.revoxelize();
}

AsyncTasksInfo VoxelizeSceneTask::initialize(CompositorPass& pass)
{
	if (pass.info.entry != "Voxelize")
		return {};

	voxelization.initialize(provider.renderSystem, provider.params, provider.resources, shadowMaps, renderWorld, pass.targets.front().texture->format);

	frameCbuffer = provider.resources.shaderBuffers.CreateCbufferResource(sizeof(Matrix), "FrameInfo");

	AsyncTasksInfo tasks;
	eventBegin = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	eventFinish = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	voxelizeCommands = provider.renderSystem.core.CreateCommandList(L"Voxelize", PixColor::Voxelize);

	worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				auto marker = provider.renderSystem.core.StartCommandList(voxelizeCommands);

				voxelization.voxelizeCascades(voxelizeCommands, pass.targets.front(), ctx);

				marker.close();

				SetEvent(eventFinish);
			}
		});

	tasks.emplace_back(eventFinish, voxelizeCommands);

	return tasks;
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CommandsData& syncCommands, CompositorPass& pass)
{
	if (pass.info.entry == "EndFrame")
	{
		voxelization.transitionForCompute(syncCommands.commandList);
	}
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CompositorPass& pass)
{
	{
		auto m = XMMatrixMultiplyTranspose(renderCtx.camera->getViewMatrix(), renderCtx.camera->getProjectionMatrixNoReverse());
		Matrix data;
		XMStoreFloat4x4(&data, m);
		auto& cbufferResource = *frameCbuffer.data[provider.params.frameIndex];
		memcpy(cbufferResource.Memory(), &data, sizeof(data));
	}

	ctx = renderCtx;
	SetEvent(eventBegin);
}

void VoxelizeSceneTask::runCompute(RenderContext& renderCtx, CommandsData& computeCommands, CompositorPass& pass)
{
	voxelization.runBounces(computeCommands, renderCtx, params);
}
