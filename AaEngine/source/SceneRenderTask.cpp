#include "SceneRenderTask.h"
#include "AaRenderables.h"
#include "AaSceneManager.h"

SceneRenderTask::SceneRenderTask()
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
}

AsyncTasksInfo SceneRenderTask::initialize(AaRenderSystem* renderSystem, RenderQueue* queue)
{
	sceneQueue = queue;
	
	scene.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	scene.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	scene.commands = renderSystem->CreateCommandList(L"Scene");

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

AsyncTasksInfo SceneRenderTask::initializeEarlyZ(AaRenderSystem* renderSystem, RenderQueue* queue)
{
	depthQueue = queue;

	earlyZ.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.commands = renderSystem->CreateCommandList(L"EarlyZ");

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
	ctx.camera->updateMatrix();
	Renderables::Get().updateVisibility(ctx.camera->prepareFrustum(), sceneInfo.visibility);
	Renderables::Get().updateWVPMatrix(ctx.camera->getViewProjectionMatrix(), sceneInfo.visibility, sceneInfo.wvpMatrix);

	if (earlyZ.eventBegin)
		SetEvent(earlyZ.eventBegin);

	ctx.renderSystem->StartCommandList(scene.commands);

	ctx.target->PrepareAsTarget(scene.commands.commandList, ctx.renderSystem->frameIndex, true, true, !earlyZ.eventBegin);
	sceneQueue->renderObjects(*ctx.camera, sceneInfo, ctx.params, scene.commands.commandList, ctx.renderSystem->frameIndex);

	ctx.target->PrepareAsView(scene.commands.commandList, ctx.renderSystem->frameIndex);
}

void SceneRenderTask::renderEarlyZ()
{
	ctx.renderSystem->StartCommandList(earlyZ.commands);

	ctx.target->PrepareAsDepthTarget(earlyZ.commands.commandList, ctx.renderSystem->frameIndex);
	depthQueue->renderObjects(*ctx.camera, sceneInfo, ctx.params, earlyZ.commands.commandList, ctx.renderSystem->frameIndex);
}
