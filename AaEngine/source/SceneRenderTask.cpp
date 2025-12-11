#include "SceneRenderTask.h"
#include "RenderObject.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include "ModelResources.h"
#include "DrawPrimitives.h"

SceneRenderTask* instance = nullptr;

SceneRenderTask::SceneRenderTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s), picker(p.renderSystem)
{
	renderables = sceneMgr.getRenderables(Order::Normal);
	transparent.renderables = sceneMgr.getRenderables(Order::Transparent);
	instance = this;
}

SceneRenderTask::~SceneRenderTask()
{
	instance = nullptr;
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
		sceneQueue = sceneMgr.createQueue(pass.target.textureSet->formats, MaterialTechnique::NoDepthWrite);

		scene.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		scene.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		scene.commands = provider.renderSystem.core.CreateCommandList(L"Scene", PixColor::SceneRender);

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
		transparent.work.commands = provider.renderSystem.core.CreateCommandList(L"SceneTransparent", PixColor::SceneTransparent);

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
	else if (pass.info.entry == "Wireframe")
	{
		wireframeQueue = sceneMgr.createQueue(pass.target.textureSet->formats, MaterialTechnique::Wireframe, Order::Normal);
	}

	return tasks;
}

AsyncTasksInfo SceneRenderTask::initializeEarlyZ(CompositorPass& pass)
{
	depthQueue = sceneMgr.createQueue({}, MaterialTechnique::Depth);

	earlyZ.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.commands = provider.renderSystem.core.CreateCommandList(L"EarlyZ", PixColor::EarlyZ);

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
	if (pass.info.entry == "EarlyZ")
	{
		//just init, early z starts from Opaque thread
		ctx = renderCtx;
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
	else if (pass.info.entry == "Wireframe")
	{
		renderWireframe(pass, cmd);
	}
	else if (pass.info.entry == "Debug")
	{
		renderDebug(pass, cmd);
	}
}

void SceneRenderTask::resize(CompositorPass& pass)
{
	if (pass.info.entry == "Editor")
		picker.initializeGpuResources();
}

bool SceneRenderTask::writesSyncCommands(CompositorPass& pass) const
{
	return pass.info.entry == "Editor" || pass.info.entry == "Wireframe" || pass.info.entry == "Debug";
}

SceneRenderTask& SceneRenderTask::Get()
{
	return *instance;
}

void SceneRenderTask::renderScene(CompositorPass& pass)
{
	renderables->updateVisibility(*ctx.camera, sceneVisibility);

	if (earlyZ.eventBegin)
		SetEvent(earlyZ.eventBegin);

	auto marker = provider.renderSystem.core.StartCommandList(scene.commands);

	pass.target.textureSet->PrepareAsTarget(scene.commands.commandList, true, TransitionFlags::DepthPrepareRead);

	if (enabledWireframe)
		return;

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
	auto commandList = transparent.work.commands.commandList;

	transparent.renderables->updateVisibility(*ctx.camera, transparent.sceneVisibility);

	auto marker = provider.renderSystem.core.StartCommandList(transparent.work.commands);

	for (auto& i : pass.inputs)
	{
		i.texture->PrepareAsView(commandList, i.previousState);
	}

	// first lets copy input opaque depth to separate depth buffer
	{
		auto opaqueDepth = pass.inputs[0];
		auto& ourDepth = pass.target.textureSet->depthState;

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(
				opaqueDepth.texture->texture.Get(),
				opaqueDepth.previousState,
				D3D12_RESOURCE_STATE_COPY_SOURCE
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				ourDepth.texture->texture.Get(),
				ourDepth.previousState,
				D3D12_RESOURCE_STATE_COPY_DEST
			)
		};
		commandList->ResourceBarrier(2, barriers);

		commandList->CopyResource(ourDepth.texture->texture.Get(), opaqueDepth.texture->texture.Get());

		CD3DX12_RESOURCE_BARRIER barriersBack[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(
				opaqueDepth.texture->texture.Get(),
				D3D12_RESOURCE_STATE_COPY_SOURCE,
				opaqueDepth.previousState
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				ourDepth.texture->texture.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				ourDepth.previousState
			)
		};
		commandList->ResourceBarrier(2, barriersBack);
	}

	pass.target.textureSet->PrepareAsTarget(commandList, true, TransitionFlags::UseDepth);

	ShaderConstantsProvider constants(provider.params, transparent.sceneVisibility, *ctx.camera, *pass.target.texture);
	transparent.transparentQueue->renderObjects(constants, commandList);
}

void SceneRenderTask::renderEditor(CompositorPass& pass, CommandsData& cmd)
{
	picker.update(cmd.commandList, provider, *ctx.camera, sceneMgr);

	pass.target.textureSet->PrepareAsTarget(cmd.commandList, false, TransitionFlags::DepthPrepareRead);

	if (!picker.active.empty())
	{
		BoundingBoxDraw bboxDraw(provider.resources, { pass.target.texture->format });

		ShaderConstantsProvider constants(provider.params, {}, *ctx.camera, *pass.target.texture);

		for (auto selectedId : picker.active)
		{
			if (auto selectedEntity = sceneMgr.getEntity(selectedId))
			{
				bboxDraw.renderObjectAligned(cmd.commandList, constants, selectedEntity);
			}
		}
	}
}

void SceneRenderTask::renderWireframe(CompositorPass& pass, CommandsData& cmd)
{
	if (!enabledWireframe)
		return;

	pass.target.textureSet->PrepareAsTarget(cmd.commandList, true, TransitionFlags::DepthContinue);

	ShaderConstantsProvider constants(provider.params, sceneVisibility, *ctx.camera, *pass.target.texture);

	wireframeQueue->renderObjects(constants, cmd.commandList);
}

void SceneRenderTask::renderDebug(CompositorPass& pass, CommandsData& cmd)
{
	pass.target.textureSet->PrepareAsTarget(cmd.commandList, false, TransitionFlags::NoDepth);

	if (!showVoxelsEnabled)
		return;

	RenderObjectsStorage tmpStorage;
	SceneEntity entity(tmpStorage, "");
	RenderObjectsVisibilityData visibility{ { true } };

	showVoxelsUpdate(entity, *ctx.camera);

	ShaderConstantsProvider constants(provider.params, visibility, *ctx.camera, *pass.target.texture);

	RenderQueue queue{ pass.target.textureSet->formats, MaterialTechnique::Default };
	queue.update({ EntityChange::Add, Order::Normal, &entity }, provider.resources);
	queue.renderObjects(constants, cmd.commandList);
}

void SceneRenderTask::showVoxelsInfo(bool show)
{
	showVoxelsEnabled = show;
}

void SceneRenderTask::showVoxelsUpdate(SceneEntity& debugVoxel, Camera& camera)
{
	auto orientation = camera.getOrientation();
	auto pos = camera.getPosition() - orientation * Vector3(0, 5.f, 0) + camera.getCameraDirection() * 1.75;

	debugVoxel.material = provider.resources.materials.getMaterial("VisualizeVoxelTexture");
	debugVoxel.geometry.fromModel(*provider.resources.models.getLoadedModel("box.mesh", ResourceGroup::Core));
	debugVoxel.setTransformation({ orientation, pos, Vector3(10, 10, 1) }, true);
}
