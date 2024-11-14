#include "SceneRenderTask.h"
#include "AaRenderables.h"
#include "AaSceneManager.h"

SceneRenderTask::SceneRenderTask(RenderProvider p, AaSceneManager& s) : CompositorTask(p, s)
{
	renderables = sceneMgr.getRenderables(Order::Normal);
}

SceneRenderTask::~SceneRenderTask()
{
	running = false;

	if (scene.eventBegin)
	{
		SetEvent(scene.eventBegin);
		scene.worker.join();
		scene.commands.deinit();
		CloseHandle(scene.eventBegin);
		CloseHandle(scene.eventFinish);
	}

	if (earlyZ.eventBegin)
	{
		SetEvent(earlyZ.eventBegin);
		earlyZ.worker.join();
		earlyZ.commands.deinit();
		CloseHandle(earlyZ.eventBegin);
		CloseHandle(earlyZ.eventFinish);
	}
}

AsyncTasksInfo SceneRenderTask::initialize(CompositorPass& pass)
{
	sceneQueue = sceneMgr.createQueue(pass.target.texture->formats);
	
	scene.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	scene.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	scene.commands = provider.renderSystem->CreateCommandList(L"Scene");

	scene.worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(scene.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				renderScene(pass);

				SetEvent(scene.eventFinish);
			}
		});

	AsyncTasksInfo tasks;

	if (!pass.info.params.empty() && pass.info.params.front() == "EarlyZ")
	{
		tasks = initializeEarlyZ(pass);
	}

	tasks.push_back({ scene.eventFinish, scene.commands });

	return tasks;
}

AsyncTasksInfo SceneRenderTask::initializeEarlyZ(CompositorPass& pass)
{
	depthQueue = sceneMgr.createQueue({}, MaterialTechnique::Depth);

	earlyZ.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.commands = provider.renderSystem->CreateCommandList(L"EarlyZ");

	earlyZ.worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(earlyZ.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				renderEarlyZ(pass);

				SetEvent(earlyZ.eventFinish);
			}
		});

	return { { earlyZ.eventFinish, earlyZ.commands } };
}

void SceneRenderTask::run(RenderContext& renderCtx, CommandsData&, CompositorPass&)
{
	ctx = renderCtx;
	SetEvent(scene.eventBegin);
}

void SceneRenderTask::renderScene(CompositorPass& pass)
{
	renderables->updateRenderInformation(*ctx.camera, sceneInfo);

	if (earlyZ.eventBegin)
		SetEvent(earlyZ.eventBegin);

	provider.renderSystem->StartCommandList(scene.commands);

	pass.target.texture->PrepareAsTarget(scene.commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true, true, !earlyZ.eventBegin);

	ShaderConstantsProvider constants(sceneInfo, *ctx.camera, *pass.target.texture);
	sceneQueue->renderObjects(constants, provider.params, scene.commands.commandList, provider.renderSystem->frameIndex);

	//ctx.target->PrepareAsView(scene.commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void SceneRenderTask::renderEarlyZ(CompositorPass& pass)
{
	provider.renderSystem->StartCommandList(earlyZ.commands);

	pass.target.texture->PrepareAsDepthTarget(earlyZ.commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	ShaderConstantsProvider constants(sceneInfo, *ctx.camera, *pass.target.texture);
	depthQueue->renderObjects(constants, provider.params, earlyZ.commands.commandList, provider.renderSystem->frameIndex);
}

SceneRenderTransparentTask::SceneRenderTransparentTask(RenderProvider p, AaSceneManager& s) : CompositorTask(p, s)
{
	renderables = sceneMgr.getRenderables(Order::Transparent);
}

SceneRenderTransparentTask::~SceneRenderTransparentTask()
{
	running = false;

	if (transparent.eventBegin)
	{
		SetEvent(transparent.eventBegin);
		transparent.worker.join();
		transparent.commands.deinit();
		CloseHandle(transparent.eventBegin);
		CloseHandle(transparent.eventFinish);
	}
}

AsyncTasksInfo SceneRenderTransparentTask::initialize(CompositorPass& pass)
{
	transparentQueue = sceneMgr.createQueue({ pass.target.texture->formats.front() }, MaterialTechnique::Default, Order::Transparent);

	transparent.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	transparent.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	transparent.commands = provider.renderSystem->CreateCommandList(L"SceneTransparent");

	transparent.worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(transparent.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				renderTransparentScene(pass);

				SetEvent(transparent.eventFinish);
			}
		});

	return { { transparent.eventFinish, transparent.commands } };
}

void SceneRenderTransparentTask::run(RenderContext& renderCtx, CommandsData&, CompositorPass&)
{
	ctx = renderCtx;
	SetEvent(transparent.eventBegin);
}

void SceneRenderTransparentTask::renderTransparentScene(CompositorPass& pass)
{
	renderables->updateRenderInformation(*ctx.camera, sceneInfo);

	provider.renderSystem->StartCommandList(transparent.commands);

	pass.target.texture->PrepareAsSingleTarget(transparent.commands.commandList, provider.renderSystem->frameIndex, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, false, true, false);

	ShaderConstantsProvider constants(sceneInfo, *ctx.camera, *pass.target.texture);
	transparentQueue->renderObjects(constants, provider.params, transparent.commands.commandList, provider.renderSystem->frameIndex);

	//ctx.target->PrepareAsSingleView(transparent.commands.commandList, provider.renderSystem->frameIndex, 0, D3D12_RESOURCE_STATE_RENDER_TARGET);
}
