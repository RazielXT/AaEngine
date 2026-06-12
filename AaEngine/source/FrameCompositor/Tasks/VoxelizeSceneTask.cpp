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

CompositorTask::Execution VoxelizeSceneTask::getExecution(CompositorPass& pass) const
{
	if (pass.info.entry == "EndFrame")
		return { RecordMode::Inline, Queue::Graphics };
	if (pass.info.entry == "Bounces")
		return { RecordMode::Inline, Queue::Compute };

	return { RecordMode::Threaded, Queue::Graphics };
}

void VoxelizeSceneTask::revoxelize()
{
	voxelization.revoxelize();
}

AsyncTasksInfo VoxelizeSceneTask::buildAsyncTasks(CompositorPass& pass)
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

void VoxelizeSceneTask::recordCommands(RenderContext& renderCtx, CommandsData& commands, CompositorPass& pass)
{
	if (pass.info.entry == "EndFrame")
	{
		voxelization.transitionForCompute(commands.commandList);
	}
	else if (pass.info.entry == "Bounces")
	{
		voxelization.runBounces(commands, renderCtx, params);
	}
}

void VoxelizeSceneTask::update(RenderContext& renderCtx, CompositorPass& pass)
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
