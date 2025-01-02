#include "SceneRenderTask.h"
#include "RenderObject.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include "ModelResources.h"
#include "DrawPrimitives.h"

SceneRenderTask::SceneRenderTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
{
	renderables = sceneMgr.getRenderables(Order::Normal);
	transparent.renderables = sceneMgr.getRenderables(Order::Transparent);
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

	if (transparent.work.eventBegin)
	{
		SetEvent(transparent.work.eventBegin);
		transparent.work.worker.join();
		transparent.work.commands.deinit();
		CloseHandle(transparent.work.eventBegin);
		CloseHandle(transparent.work.eventFinish);
	}
}

AsyncTasksInfo SceneRenderTask::initialize(CompositorPass& pass)
{
	AsyncTasksInfo tasks;

	if (pass.info.entry == "EarlyZ")
	{
		tasks = initializeEarlyZ(pass);
	}
	else if (pass.info.entry == "Opaque")
	{
		sceneQueue = sceneMgr.createQueue(pass.target.textureSet->formats);

		scene.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		scene.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		scene.commands = provider.renderSystem.core.CreateCommandList(L"Scene");

		scene.worker = std::thread([this, &pass]
			{
				while (WaitForSingleObject(scene.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
				{
					renderScene(pass);

					SetEvent(scene.eventFinish);
				}
			});

		tasks = {{ scene.eventFinish, scene.commands }};
	}
	else if (pass.info.entry == "Transparent")
	{
		transparent.transparentQueue = sceneMgr.createQueue(pass.target.textureSet->formats, MaterialTechnique::Default, Order::Transparent);

		transparent.work.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		transparent.work.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		transparent.work.commands = provider.renderSystem.core.CreateCommandList(L"SceneTransparent");

		transparent.work.worker = std::thread([this, &pass]
			{
				while (WaitForSingleObject(transparent.work.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
				{
					renderTransparentScene(pass);

					SetEvent(transparent.work.eventFinish);
				}
			});

		tasks = { { transparent.work.eventFinish, transparent.work.commands } };
	}

	return tasks;
}

AsyncTasksInfo SceneRenderTask::initializeEarlyZ(CompositorPass& pass)
{
	depthQueue = sceneMgr.createQueue({}, MaterialTechnique::Depth);

	earlyZ.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.commands = provider.renderSystem.core.CreateCommandList(L"EarlyZ");

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

void SceneRenderTask::run(RenderContext& renderCtx, CommandsData& cmd, CompositorPass& pass)
{
	if (pass.info.entry == "Prepare")
	{
		ctx = renderCtx;

		auto& dlss = provider.renderSystem.upscale.dlss;
		if (dlss.enabled())
			ctx.camera->setPixelOffset(dlss.getJitter(), dlss.getRenderSize());
		auto& fsr = provider.renderSystem.upscale.fsr;
		if (fsr.enabled())
			ctx.camera->setPixelOffset(fsr.getJitter(), fsr.getRenderSize());
	}

	//starts from Opaque thread
	if (pass.info.entry == "EarlyZ")
	{
	}
	else if (pass.info.entry == "Opaque")
	{
		SetEvent(scene.eventBegin);
	}
	else if (pass.info.entry == "Transparent")
	{
		SetEvent(transparent.work.eventBegin);
	}
	else if (pass.info.entry == "Editor")
	{
		renderEditor(pass, cmd);
	}
}

void SceneRenderTask::resize(CompositorPass& pass)
{
	if (pass.info.entry == "Prepare")
		picker.initializeGpuResources(provider.renderSystem, provider.resources);
}

bool SceneRenderTask::writesSyncCommands(CompositorPass& pass) const
{
	return pass.info.entry == "Editor";
}

void SceneRenderTask::renderScene(CompositorPass& pass)
{
	renderables->updateVisibility(*ctx.camera, sceneVisibility);

	if (earlyZ.eventBegin)
		SetEvent(earlyZ.eventBegin);

	auto marker = provider.renderSystem.core.StartCommandList(scene.commands);

	pass.target.textureSet->PrepareAsTarget(scene.commands.commandList, true, TransitionFlags::DepthContinue);

	ShaderConstantsProvider constants(provider.params, sceneVisibility, *ctx.camera, *pass.target.texture);

	sceneMgr.skybox.render(scene.commands.commandList, constants);
	
	sceneQueue->renderObjects(constants, scene.commands.commandList);
}

void SceneRenderTask::renderEarlyZ(CompositorPass& pass)
{
	auto marker = provider.renderSystem.core.StartCommandList(earlyZ.commands);

	auto depthBuffer = pass.target.texture;
	depthBuffer->PrepareAsDepthTarget(earlyZ.commands.commandList, pass.target.previousState);

	ShaderConstantsProvider constants(provider.params, sceneVisibility, *ctx.camera, *depthBuffer);
	depthQueue->renderObjects(constants, earlyZ.commands.commandList);
}

void SceneRenderTask::renderTransparentScene(CompositorPass& pass)
{
	transparent.renderables->updateVisibility(*ctx.camera, transparent.sceneVisibility);

	auto marker = provider.renderSystem.core.StartCommandList(transparent.work.commands);

	for (auto& i : pass.inputs)
	{
		i.texture->PrepareAsView(transparent.work.commands.commandList, i.previousState);
	}

	pass.target.textureSet->PrepareAsTarget(transparent.work.commands.commandList, true, TransitionFlags::DepthPrepareRead);

	ShaderConstantsProvider constants(provider.params, transparent.sceneVisibility, *ctx.camera, *pass.target.texture);
	transparent.transparentQueue->renderObjects(constants, transparent.work.commands.commandList);
}

void SceneRenderTask::renderEditor(CompositorPass& pass, CommandsData& cmd)
{
	if (picker.scheduled)
	{
		picker.rtt.PrepareAsTarget(cmd.commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

		CommandsMarker marker(cmd.commandList, "EntityPicker");
		RenderQueue idQueue = picker.createRenderQueue();
		{
			auto opaqueRenderables = sceneMgr.getRenderables(Order::Normal);

			RenderObjectsVisibilityData opaqueVisibility;
			opaqueVisibility.visibility.resize(opaqueRenderables->objectsData.objects.size(), false);
			opaqueRenderables->updateVisibility(*ctx.camera, opaqueVisibility);

			opaqueRenderables->iterateObjects([this, &idQueue, &opaqueVisibility](RenderObject& obj)
				{
					if (obj.isVisible(opaqueVisibility.visibility))
					{
						idQueue.update({ EntityChange::Add, Order::Normal, (SceneEntity*)&obj }, provider.resources);
					}
				});

			ShaderConstantsProvider constants(provider.params, opaqueVisibility, *ctx.camera, picker.rtt);
			idQueue.renderObjects(constants, cmd.commandList);
		}
		idQueue.reset();

		auto pickTransparent = [this]()
			{
				auto currentPickTime = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
				constexpr auto transparentSkipMaxDelay = std::chrono::milliseconds(500);
				static auto lastPickTime = currentPickTime - transparentSkipMaxDelay;

				bool pick = !(currentPickTime - lastPickTime < transparentSkipMaxDelay && picker.lastPickWasTransparent());

				lastPickTime = currentPickTime;
				return pick;
			};

		if (pickTransparent())
		{
			auto transparentRenderables = sceneMgr.getRenderables(Order::Transparent);

			RenderObjectsVisibilityData transparentVisibility;
			transparentVisibility.visibility.resize(transparentRenderables->objectsData.objects.size(), false);
			transparentRenderables->updateVisibility(*ctx.camera, transparentVisibility);

			idQueue.reset();
			transparentRenderables->iterateObjects([this, &idQueue, &transparentVisibility](RenderObject& obj)
				{
					if (obj.isVisible(transparentVisibility.visibility))
					{
						idQueue.update({ EntityChange::Add, Order::Normal, (SceneEntity*)&obj }, provider.resources);
					}
				});

			ShaderConstantsProvider constants(provider.params, transparentVisibility, *ctx.camera, picker.rtt);
			idQueue.renderObjects(constants, cmd.commandList);
		}

		picker.scheduleEntityIdRead(cmd.commandList);
	}

	pass.target.textureSet->PrepareAsTarget(cmd.commandList, false, TransitionFlags::DepthPrepareRead);

	if (!picker.active.empty())
	{
		BoundingBoxDraw bboxDraw(provider.resources, { pass.target.texture->format });

		ShaderConstantsProvider constants(provider.params, {}, *ctx.camera, picker.rtt);

		for (auto selectedId : picker.active)
		{
			if (auto selectedEntity = sceneMgr.getEntity(selectedId))
			{
				bboxDraw.renderObjectAligned(cmd.commandList, constants, selectedEntity);
			}
		}
	}
}
