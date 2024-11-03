#include "ShadowsRenderTask.h"
#include "AaRenderables.h"
#include "AaSceneManager.h"
#include "ShadowMap.h"

ShadowsRenderTask::ShadowsRenderTask(RenderProvider p, AaShadowMap& shadows) : shadowMaps(shadows), provider(p)
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
	depthQueue = sceneMgr->createQueue({}, MaterialTechnique::Depth);

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
					provider.renderSystem->StartCommandList(shadow.commands);

					auto& info = shadow.renderablesData;
					ctx.renderables->updateRenderInformation(shadowMaps.camera[idx], info);

					shadowMaps.texture[idx].PrepareAsDepthTarget(shadow.commands.commandList, provider.renderSystem->frameIndex);

					depthQueue->renderObjects(shadowMaps.camera[idx], info, provider.params, shadow.commands.commandList, provider.renderSystem->frameIndex);

					shadowMaps.texture[idx].PrepareAsDepthView(shadow.commands.commandList, provider.renderSystem->frameIndex);

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
