#include "ShadowsRenderTask.h"
#include "AaRenderables.h"
#include "AaSceneManager.h"
#include "ShadowMap.h"

ShadowsRenderTask::ShadowsRenderTask(AaShadowMap& shadows) : shadowMaps(shadows)
{
}

ShadowsRenderTask::~ShadowsRenderTask()
{
	running = false;

	for (auto& shadow : shadowsData)
	{
		if (shadow.eventBegin)
		{
			SetEvent(shadow.eventBegin);
			shadow.worker.join();
			shadow.commands.deinit();
			CloseHandle(shadow.eventBegin);
			CloseHandle(shadow.eventFinish);
		}
	}
}

AsyncTasksInfo ShadowsRenderTask::initialize(AaRenderSystem* renderSystem, AaSceneManager* sceneMgr)
{
	depthQueue = sceneMgr->createQueue({}, MaterialVariant::Depth);

	AsyncTasksInfo tasks;

	for (auto& shadow : shadowsData)
	{
		shadow.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		shadow.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		shadow.commands = renderSystem->CreateCommandList(L"Shadows");

		shadow.worker = std::thread([this, idx = tasks.size()]
			{
				auto& shadow = shadowsData[idx];

				while (WaitForSingleObject(shadow.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
				{
					ctx.renderSystem->StartCommandList(shadow.commands);

					thread_local RenderInformation info;
					ctx.renderables->updateVisibility(shadowMaps.camera[idx].prepareOrientedBox(), info.visibility);
					ctx.renderables->updateWVPMatrix(shadowMaps.camera[idx].getViewProjectionMatrix(), info.visibility, info.wvpMatrix);

					shadowMaps.texture[idx].PrepareAsDepthTarget(shadow.commands.commandList, ctx.renderSystem->frameIndex);

					depthQueue->renderObjects(shadowMaps.camera[idx], info, ctx.params, shadow.commands.commandList, ctx.renderSystem->frameIndex);

					shadowMaps.texture[idx].PrepareAsDepthView(shadow.commands.commandList, ctx.renderSystem->frameIndex);

					SetEvent(shadow.eventFinish);
				}
			});

		tasks.emplace_back(shadow.eventFinish, shadow.commands);
	}

	return tasks;
}

void ShadowsRenderTask::prepare(RenderContext& renderCtx, CommandsData&)
{
	ctx = renderCtx;

	for (auto& shadow : shadowsData)
	{
		SetEvent(shadow.eventBegin);
	}
}
