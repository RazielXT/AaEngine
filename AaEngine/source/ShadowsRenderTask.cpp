#include "ShadowsRenderTask.h"
#include "RenderObject.h"
#include "SceneManager.h"
#include "ShadowMaps.h"

ShadowsRenderTask::ShadowsRenderTask(RenderProvider p, SceneManager& s, ShadowMaps& shadows) : shadowMaps(shadows), CompositorTask(p, s)
{
	for (auto& shadow : cascades)
	{
		shadow.renderables = sceneMgr.getRenderables(Order::Normal);
	}
	maxShadow.renderables = sceneMgr.getRenderables(Order::Normal);
}

ShadowsRenderTask::~ShadowsRenderTask()
{
	running = false;

	for (auto& shadow : cascades)
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

	if (maxShadow.eventBegin)
	{
		SetEvent(maxShadow.eventBegin);
		maxShadow.worker.join();
		maxShadow.commands.deinit();
		CloseHandle(maxShadow.eventBegin);
		CloseHandle(maxShadow.eventFinish);
	}
}

AsyncTasksInfo ShadowsRenderTask::initialize(CompositorPass&)
{
	depthQueue = sceneMgr.createQueue({}, MaterialTechnique::DepthShadowmap);

	AsyncTasksInfo tasks;

	for (auto& shadow : cascades)
	{
		shadow.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		shadow.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		shadow.commands = provider.renderSystem.core.CreateCommandList(L"Shadows", PixColor::Shadows);

		shadow.worker = std::thread([this, idx = tasks.size()]
			{
				auto& shadow = cascades[idx];
				auto& cascade = shadowMaps.cascades[idx];
				UINT counter = 0;

				while (WaitForSingleObject(shadow.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
				{
					auto marker = provider.renderSystem.core.StartCommandList(shadow.commands);

					if (!cascade.update)
					{
						marker.close();
						SetEvent(shadow.eventFinish);
						continue;
					}

					auto& sceneInfo = shadow.renderablesData;

					shadow.renderables->updateVisibility(cascade.camera, sceneInfo);

					cascade.texture.PrepareAsDepthTarget(shadow.commands.commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

					ShaderConstantsProvider constants(provider.params, sceneInfo, cascade.camera, *ctx.camera, cascade.texture);
					depthQueue->renderObjects(constants, shadow.commands.commandList);

					cascade.texture.PrepareAsView(shadow.commands.commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

					marker.close();
					SetEvent(shadow.eventFinish);
				}
			});

		tasks.emplace_back(shadow.eventFinish, shadow.commands);
	}

	{
		maxShadow.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		maxShadow.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		maxShadow.commands = provider.renderSystem.core.CreateCommandList(L"MaxShadows", PixColor::Shadows);

		maxShadow.worker = std::thread([this]
			{
				auto& shadow = maxShadow;
				auto& cascade = shadowMaps.maxShadow;
				UINT counter = 0;

				while (WaitForSingleObject(shadow.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
				{
					auto marker = provider.renderSystem.core.StartCommandList(shadow.commands);

					if (!cascade.update)
					{
						marker.close();
						SetEvent(shadow.eventFinish);
						continue;
					}

					auto& sceneInfo = shadow.renderablesData;

					shadow.renderables->updateVisibility(cascade.camera, sceneInfo);

					cascade.texture.PrepareAsDepthTarget(shadow.commands.commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

					ShaderConstantsProvider constants(provider.params, sceneInfo, cascade.camera, *ctx.camera, cascade.texture);
					depthQueue->renderObjects(constants, shadow.commands.commandList);

					cascade.texture.PrepareAsView(shadow.commands.commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

					marker.close();
					SetEvent(shadow.eventFinish);
				}
			});

		tasks.emplace_back(maxShadow.eventFinish, maxShadow.commands);
	}

	return tasks;
}

void ShadowsRenderTask::run(RenderContext& renderCtx, CommandsData&, CompositorPass&)
{
	ctx = renderCtx;

	for (auto& shadow : cascades)
	{
		SetEvent(shadow.eventBegin);
	}
	SetEvent(maxShadow.eventBegin);
}
