#include "SceneRenderTask.h"
#include "RenderObject.h"
#include "SceneManager.h"

SceneRenderTask::SceneRenderTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
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
	sceneQueue = sceneMgr.createQueue(pass.target.textureSet->formats);
	
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
	auto& dlss = provider.renderSystem->dlss;
	if (dlss.enabled())
		ctx.camera->setPixelOffset(dlss.getJitter(), dlss.getRenderSize());

	renderables->updateVisibility(*ctx.camera, sceneInfo);

	if (earlyZ.eventBegin)
		SetEvent(earlyZ.eventBegin);

	auto marker = provider.renderSystem->StartCommandList(scene.commands);

	pass.target.textureSet->PrepareAsTarget(scene.commands.commandList, true, TransitionFlags::DepthContinue);

	ShaderConstantsProvider constants(provider.params, sceneInfo, *ctx.camera, *pass.target.texture);
	sceneQueue->renderObjects(constants, scene.commands.commandList);
}

void SceneRenderTask::renderEarlyZ(CompositorPass& pass)
{
	auto marker = provider.renderSystem->StartCommandList(earlyZ.commands);

	auto depthState = pass.target.textureSet->depthState;
	depthState.texture->PrepareAsDepthTarget(earlyZ.commands.commandList, depthState.previousState);

	ShaderConstantsProvider constants(provider.params, sceneInfo, *ctx.camera, *depthState.texture);
	depthQueue->renderObjects(constants, earlyZ.commands.commandList);
}

SceneRenderTransparentTask::SceneRenderTransparentTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
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
	transparentQueue = sceneMgr.createQueue(pass.target.textureSet->formats, MaterialTechnique::Default, Order::Transparent);

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
	renderables->updateVisibility(*ctx.camera, sceneInfo);

	auto marker = provider.renderSystem->StartCommandList(transparent.commands);

	for (auto& i : pass.inputs)
	{
		i.texture->PrepareAsView(transparent.commands.commandList, i.previousState);
	}

	pass.target.textureSet->PrepareAsTarget(transparent.commands.commandList, true, TransitionFlags::DepthPrepareRead);

	ShaderConstantsProvider constants(provider.params, sceneInfo, *ctx.camera, *pass.target.texture);
	transparentQueue->renderObjects(constants, transparent.commands.commandList);
}
