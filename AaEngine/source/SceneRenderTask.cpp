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
			sceneQueue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Default);

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
		transparent.transparentQueue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Default, Order::Transparent);

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
		wireframeQueue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Wireframe, Order::Normal);
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

void SceneRenderTask::run(RenderContext& renderCtx, CompositorPass& pass)
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
}

void SceneRenderTask::run(RenderContext& renderCtx, CommandsData& cmd, CompositorPass& pass)
{
	if (pass.info.entry == "Editor")
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

	pass.mrt->PrepareAsTarget(scene.commands.commandList, pass.targets, true, TransitionFlags::UseDepth);

	if (enabledWireframe)
		return;

	ShaderConstantsProvider constants(provider.params, sceneVisibility, *ctx.camera, *pass.mrt);

	sceneMgr.skybox.render(scene.commands.commandList, constants);
	
	sceneQueue->renderObjects(constants, scene.commands.commandList);
}

void SceneRenderTask::renderEarlyZ(CompositorPass& pass)
{
	auto marker = provider.renderSystem.core.StartCommandList(earlyZ.commands);

	auto& depth = pass.targets.front();
	depth.texture->PrepareAsDepthTarget(earlyZ.commands.commandList, depth.previousState);

	ShaderConstantsProvider constants(provider.params, sceneVisibility, *ctx.camera, *depth.texture);
	depthQueue->renderObjects(constants, earlyZ.commands.commandList);

	depth.texture->Transition(earlyZ.commands.commandList, depth.state, depth.nextState);
}

void SceneRenderTask::renderTransparentScene(CompositorPass& pass)
{
	auto commandList = transparent.work.commands.commandList;

	transparent.renderables->updateVisibility(*ctx.camera, transparent.sceneVisibility);

	auto marker = provider.renderSystem.core.StartCommandList(transparent.work.commands);

	TextureTransitions<5>(pass.inputs, commandList);

	// first lets copy input opaque depth to separate depth buffer
	{
		auto opaqueDepth = pass.inputs[0];
		auto& ourDepth = pass.targets.back();

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

	sceneMgr.water.prepareForRendering(commandList);

	pass.mrt->PrepareAsTarget(commandList, pass.targets, true, TransitionFlags::UseDepth);

	ShaderConstantsProvider constants(provider.params, transparent.sceneVisibility, *ctx.camera, *pass.mrt);
	transparent.transparentQueue->renderObjects(constants, commandList);

	sceneMgr.water.prepareAfterRendering(commandList);
}

void SceneRenderTask::renderEditor(CompositorPass& pass, CommandsData& cmd)
{
	picker.update(cmd.commandList, provider, *ctx.camera, sceneMgr);

	pass.mrt->PrepareAsTarget(cmd.commandList, pass.targets, false, TransitionFlags::DepthPrepareRead);

	if (!picker.active.empty())
	{
		BoundingBoxDraw bboxDraw(provider.resources, { pass.mrt->formats });

		ShaderConstantsProvider constants(provider.params, {}, *ctx.camera, *pass.mrt);

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
	{
		pass.mrt->PrepareAsTarget(cmd.commandList, pass.targets, false, TransitionFlags::UseDepth);
		return;
	}

	pass.mrt->PrepareAsTarget(cmd.commandList, pass.targets, true, TransitionFlags::UseDepth);

	ShaderConstantsProvider constants(provider.params, sceneVisibility, *ctx.camera, *pass.mrt);

	wireframeQueue->renderObjects(constants, cmd.commandList);
}

void SceneRenderTask::renderDebug(CompositorPass& pass, CommandsData& cmd)
{
	auto& target = pass.targets.front();
	target.texture->PrepareAsRenderTarget(cmd.commandList, target.previousState);

	if (!showVoxelsEnabled)
		return;

	RenderObjectsStorage tmpStorage;
	SceneEntity entity(tmpStorage, "");
	RenderObjectsVisibilityData visibility{ { true } };

	updateVoxelsDebugView(entity, *ctx.camera);

	ShaderConstantsProvider constants(provider.params, visibility, *ctx.camera, *target.texture);

	RenderQueue queue{ { target.texture->format }, MaterialTechnique::Default };
	queue.update({ EntityChange::Add, Order::Normal, &entity }, provider.resources);
	queue.renderObjects(constants, cmd.commandList);
}

void SceneRenderTask::showVoxels(bool show)
{
	showVoxelsEnabled = show;
}

void SceneRenderTask::updateVoxelsDebugView(SceneEntity& debugVoxel, Camera& camera)
{
	auto orientation = camera.getOrientation();
	auto pos = camera.getPosition() - orientation * Vector3(0, 5.f, 0) + camera.getCameraDirection() * 1.75;

	debugVoxel.material = provider.resources.materials.getMaterial("VisualizeVoxelTexture");
	debugVoxel.geometry.fromModel(*provider.resources.models.getLoadedModel("box.mesh", ResourceGroup::Core));
	debugVoxel.setTransformation({ orientation, pos, Vector3(10, 10, 1) }, true);
}
