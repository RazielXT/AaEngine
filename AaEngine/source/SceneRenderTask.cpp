#include "SceneRenderTask.h"
#include "AaRenderables.h"
#include "AaSceneManager.h"

SceneRenderTask::SceneRenderTask(RenderProvider p) : provider(p)
{
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

	if (transparent.eventBegin)
	{
		SetEvent(transparent.eventBegin);
		transparent.worker.join();
		transparent.commands.deinit();
		CloseHandle(transparent.eventBegin);
		CloseHandle(transparent.eventFinish);
	}
}

AsyncTasksInfo SceneRenderTask::initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target)
{
	sceneQueue = sceneMgr->createQueue(target->formats);
	
	scene.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	scene.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	scene.commands = provider.renderSystem->CreateCommandList(L"Scene");

	scene.worker = std::thread([this]
		{
			while (WaitForSingleObject(scene.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				renderScene();

				SetEvent(scene.eventFinish);
			}
		});

	return { { scene.eventFinish, scene.commands } };
}

AsyncTasksInfo SceneRenderTask::initializeTransparent(AaSceneManager* sceneMgr, RenderTargetTexture* target)
{
	transparentQueue = sceneMgr->createQueue(target->formats, MaterialTechnique::Default, Order::Transparent);

	transparent.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	transparent.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	transparent.commands = provider.renderSystem->CreateCommandList(L"SceneTransparent");

	transparent.worker = std::thread([this]
		{
			while (WaitForSingleObject(transparent.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				renderTransparentScene();

				SetEvent(transparent.eventFinish);
			}
		});

	return { { transparent.eventFinish, transparent.commands } };
}

AsyncTasksInfo SceneRenderTask::initializeEarlyZ(AaSceneManager* sceneMgr)
{
	depthQueue = sceneMgr->createQueue({}, MaterialTechnique::Depth);

	earlyZ.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.commands = provider.renderSystem->CreateCommandList(L"EarlyZ");

	earlyZ.worker = std::thread([this]
		{
			while (WaitForSingleObject(earlyZ.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				renderEarlyZ();

				SetEvent(earlyZ.eventFinish);
			}
		});

	return { { earlyZ.eventFinish, earlyZ.commands } };
}

void SceneRenderTask::prepare(RenderContext& renderCtx, CommandsData&)
{
	ctx = renderCtx;
	SetEvent(scene.eventBegin);
}

void SceneRenderTask::renderScene()
{
	ctx.renderables->updateRenderInformation(*ctx.camera, sceneInfo);

	if (earlyZ.eventBegin)
		SetEvent(earlyZ.eventBegin);
	if (transparent.eventBegin)
		SetEvent(transparent.eventBegin);

	provider.renderSystem->StartCommandList(scene.commands);

	ctx.target->PrepareAsTarget(scene.commands.commandList, provider.renderSystem->frameIndex, true, true, !earlyZ.eventBegin);
	sceneQueue->renderObjects(*ctx.camera, sceneInfo, provider.params, scene.commands.commandList, provider.renderSystem->frameIndex);

	ctx.target->PrepareAsView(scene.commands.commandList, provider.renderSystem->frameIndex);
}

void SceneRenderTask::renderTransparentScene()
{
	provider.renderSystem->StartCommandList(transparent.commands);

	ctx.target->PrepareAsSingleTarget(transparent.commands.commandList, provider.renderSystem->frameIndex, 0, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false, true, false);
	transparentQueue->renderObjects(*ctx.camera, sceneInfo, provider.params, transparent.commands.commandList, provider.renderSystem->frameIndex);

	ctx.target->PrepareAsSingleView(transparent.commands.commandList, provider.renderSystem->frameIndex, 0, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void SceneRenderTask::renderEarlyZ()
{
	provider.renderSystem->StartCommandList(earlyZ.commands);

	ctx.target->PrepareAsDepthTarget(earlyZ.commands.commandList, provider.renderSystem->frameIndex);
	depthQueue->renderObjects(*ctx.camera, sceneInfo, provider.params, earlyZ.commands.commandList, provider.renderSystem->frameIndex);
}
