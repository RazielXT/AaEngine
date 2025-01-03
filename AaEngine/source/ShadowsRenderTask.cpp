#include "ShadowsRenderTask.h"
#include "RenderObject.h"
#include "SceneManager.h"
#include "ShadowMap.h"

ShadowsRenderTask::ShadowsRenderTask(RenderProvider p, SceneManager& s, AaShadowMap& shadows) : shadowMaps(shadows), CompositorTask(p, s)
{
	for (auto& shadow : shadowsData)
	{
		shadow.renderables = sceneMgr.getRenderables(Order::Normal);
	}
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

AsyncTasksInfo ShadowsRenderTask::initialize(CompositorPass&)
{
	depthQueue = sceneMgr.createQueue({}, MaterialTechnique::DepthNonReversed);

	AsyncTasksInfo tasks;

	for (auto& shadow : shadowsData)
	{
		shadow.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		shadow.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		shadow.commands = provider.renderSystem.core.CreateCommandList(L"Shadows");

		shadow.worker = std::thread([this, idx = tasks.size()]
			{
				auto& shadow = shadowsData[idx];

				while (WaitForSingleObject(shadow.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
				{
					auto marker = provider.renderSystem.core.StartCommandList(shadow.commands);

					auto& sceneInfo = shadow.renderablesData;

					shadow.renderables->updateVisibility(shadowMaps.camera[idx], sceneInfo);

					shadowMaps.texture[idx].PrepareAsDepthTarget(shadow.commands.commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

					ShaderConstantsProvider constants(provider.params, sceneInfo, shadowMaps.camera[idx], shadowMaps.texture[idx]);
					depthQueue->renderObjects(constants, shadow.commands.commandList);

					shadowMaps.texture[idx].PrepareAsView(shadow.commands.commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

					marker.close();
					SetEvent(shadow.eventFinish);
				}
			});

		tasks.emplace_back(shadow.eventFinish, shadow.commands);
	}

	return tasks;
}

void ShadowsRenderTask::run(RenderContext& renderCtx, CommandsData&, CompositorPass&)
{
	for (auto& shadow : shadowsData)
	{
		SetEvent(shadow.eventBegin);
	}
}
