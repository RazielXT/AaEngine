#include "FrameCompositor/Tasks/SceneRenderTask.h"
#include "Scene/RenderObject.h"
#include "Scene/SceneManager.h"
#include "Resources/Material/MaterialResources.h"
#include "Resources/Model/ModelResources.h"
#include "Scene/DrawPrimitives.h"

SceneRenderTask* instance = nullptr;

SceneRenderTask::SceneRenderTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s), picker(p.renderSystem)
{
	opaque.renderables = sceneMgr.getRenderables(Order::Normal);
	forward.renderables = sceneMgr.getRenderables(Order::Post);
	transparent.renderables = sceneMgr.getRenderables(Order::Transparent);
	instance = this;
}

SceneRenderTask::~SceneRenderTask()
{
	instance = nullptr;
	running = false;

	if (opaque.work.eventBegin)
	{
		SetEvent(opaque.work.eventBegin);
		opaque.work.worker.join();
		opaque.work.commands.deinit();
		CloseHandle(opaque.work.eventBegin);
		CloseHandle(opaque.work.eventFinish);
	}

	if (earlyZ.work.eventBegin)
	{
		SetEvent(earlyZ.work.eventBegin);
		earlyZ.work.worker.join();
		earlyZ.work.commands.deinit();
		CloseHandle(earlyZ.work.eventBegin);
		CloseHandle(earlyZ.work.eventFinish);
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
		opaque.queue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Default);

		opaque.work.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		opaque.work.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		opaque.work.commands = provider.renderSystem.core.CreateCommandList(L"SceneRender", PixColor::SceneRender);

		opaque.work.worker = std::thread([this, &pass]
			{
				while (WaitForSingleObject(opaque.work.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
				{
					renderScene(pass);

					SetEvent(opaque.work.eventFinish);
				}
			});

		tasks = {{ opaque.work.eventFinish, opaque.work.commands }};
	}
	else if (pass.info.entry == "Transparent")
	{
		transparent.queue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Default, Order::Transparent);

		transparent.work.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		transparent.work.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		transparent.work.commands = provider.renderSystem.core.CreateCommandList(L"SceneRenderTransparent", PixColor::SceneTransparent);

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
		if (opaque.queue)
			__debugbreak();

		opaque.queue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Default);
		opaque.wireframeQueue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Wireframe);

		forward.queue = sceneMgr.createQueue({ pass.mrt->formats.front() }, MaterialTechnique::Default, Order::Post);
		forward.wireframeQueue = sceneMgr.createQueue({ pass.mrt->formats.front() }, MaterialTechnique::Wireframe, Order::Post);

		transparent.queue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Default, Order::Transparent);
		transparent.wireframeQueue = sceneMgr.createQueue({ pass.mrt->formats.front() }, MaterialTechnique::Wireframe, Order::Transparent);

		opaque.work.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		opaque.work.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
		opaque.work.commands = provider.renderSystem.core.CreateCommandList(L"Wireframe", PixColor::SceneRender);

		opaque.work.worker = std::thread([this, &pass]
			{
				while (WaitForSingleObject(opaque.work.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
				{
					renderWireframe(pass);

					SetEvent(opaque.work.eventFinish);
				}
			});

		tasks = { { opaque.work.eventFinish, opaque.work.commands } };
	}
	else if (pass.info.entry == "Forward")
	{
		forward.queue = sceneMgr.createQueue(pass.mrt->formats, MaterialTechnique::Default, Order::Post);
	}

	return tasks;
}

AsyncTasksInfo SceneRenderTask::initializeEarlyZ(CompositorPass& pass)
{
	earlyZ.queue = sceneMgr.createQueue({}, MaterialTechnique::Depth);

	earlyZ.work.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.work.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	earlyZ.work.commands = provider.renderSystem.core.CreateCommandList(L"EarlyZ", PixColor::EarlyZ);

	earlyZ.work.worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(earlyZ.work.eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				renderEarlyZ(pass);

				SetEvent(earlyZ.work.eventFinish);
			}
		});

	return { { earlyZ.work.eventFinish, earlyZ.work.commands } };
}

void SceneRenderTask::run(RenderContext& renderCtx, CompositorPass& pass)
{
	if (pass.info.entry == "EarlyZ")
	{
		//just init, early z starts from Opaque thread
		ctx = renderCtx;
	}
	else if (pass.info.entry == "Opaque" || pass.info.entry == "Wireframe")
	{
		SetEvent(opaque.work.eventBegin);
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
	else if (pass.info.entry == "Debug")
	{
		renderDebug(pass, cmd);
	}
	else if (pass.info.entry == "Forward")
	{
		renderForward(pass, cmd);
	}
}

void SceneRenderTask::resize(CompositorPass& pass)
{
	if (pass.info.entry == "Editor")
		picker.initializeGpuResources();
}

SceneRenderTask& SceneRenderTask::Get()
{
	return *instance;
}

void SceneRenderTask::renderWireframe(CompositorPass& pass)
{
	opaque.renderables->updateVisibility(*ctx.camera, opaque.visibility);

	if (earlyZ.work.eventBegin)
		SetEvent(earlyZ.work.eventBegin);

	auto marker = provider.renderSystem.core.StartCommandList(opaque.work.commands);

	pass.mrt->PrepareAsTarget(opaque.work.commands.commandList, pass.targets, true, TransitionFlags::UseDepth);

	ShaderConstantsProvider constants(provider.params, opaque.visibility, *ctx.camera, *pass.mrt);

	opaque.wireframeQueue->renderObjects(constants, opaque.work.commands.commandList);

	pass.mrt->PrepareSubrangeAsTarget(opaque.work.commands.commandList, 1, 0, pass.targets.back().texture);
	{
		forward.renderables->updateVisibility(*ctx.camera, forward.visibility);

		ShaderConstantsProvider constants(provider.params, forward.visibility, *ctx.camera, *pass.mrt);

		forward.wireframeQueue->renderObjects(constants, opaque.work.commands.commandList);
	}
	{
		transparent.renderables->updateVisibility(*ctx.camera, transparent.visibility);

		ShaderConstantsProvider constants(provider.params, transparent.visibility, *ctx.camera, *pass.mrt);

		transparent.wireframeQueue->renderObjects(constants, opaque.work.commands.commandList);
	}
}

void SceneRenderTask::renderScene(CompositorPass& pass)
{
	opaque.renderables->updateVisibility(*ctx.camera, opaque.visibility);

	if (earlyZ.work.eventBegin)
		SetEvent(earlyZ.work.eventBegin);

	auto marker = provider.renderSystem.core.StartCommandList(opaque.work.commands);

	pass.mrt->PrepareAsTarget(opaque.work.commands.commandList, pass.targets, true, TransitionFlags::UseDepth);

	ShaderConstantsProvider constants(provider.params, opaque.visibility, *ctx.camera, *pass.mrt);

	opaque.queue->renderObjects(constants, opaque.work.commands.commandList);
}

void SceneRenderTask::renderForward(CompositorPass& pass, CommandsData& cmd)
{
	CommandsMarker marker(cmd.commandList, "SceneRenderForward", PixColor::SceneRender);

	forward.renderables->updateVisibility(*ctx.camera, forward.visibility);

	pass.mrt->PrepareAsTarget(cmd.commandList, pass.targets, false, TransitionFlags::UseDepth);

	ShaderConstantsProvider constants(provider.params, forward.visibility, *ctx.camera, *pass.mrt);

	sceneMgr.skybox.render(cmd.commandList, constants);

	forward.queue->renderObjects(constants, cmd.commandList);
}

void SceneRenderTask::renderEarlyZ(CompositorPass& pass)
{
	auto marker = provider.renderSystem.core.StartCommandList(earlyZ.work.commands);

	auto& depth = pass.targets.front();
	depth.texture->PrepareAsDepthTarget(earlyZ.work.commands.commandList, depth.previousState);

	ShaderConstantsProvider constants(provider.params, opaque.visibility, *ctx.camera, *depth.texture);
	earlyZ.queue->renderObjects(constants, earlyZ.work.commands.commandList);

	depth.texture->Transition(earlyZ.work.commands.commandList, depth.state, depth.nextState);
}

void SceneRenderTask::renderTransparentScene(CompositorPass& pass)
{
	auto commandList = transparent.work.commands.commandList;

	transparent.renderables->updateVisibility(*ctx.camera, transparent.visibility);

	auto marker = provider.renderSystem.core.StartCommandList(transparent.work.commands);

	TextureTransitions<5>(pass.inputs, commandList);

	// first lets copy input opaque depth to separate depth buffer
	{
		auto opaqueDepth = pass.inputs[0];
		auto& ourDepth = pass.targets.back();

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(opaqueDepth.texture->texture.Get(), opaqueDepth.previousState, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(ourDepth.texture->texture.Get(), ourDepth.previousState, D3D12_RESOURCE_STATE_COPY_DEST)
		};
		commandList->ResourceBarrier(2, barriers);

		commandList->CopyResource(ourDepth.texture->texture.Get(), opaqueDepth.texture->texture.Get());

		CD3DX12_RESOURCE_BARRIER barriersBack[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(opaqueDepth.texture->texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, opaqueDepth.previousState),
			CD3DX12_RESOURCE_BARRIER::Transition(ourDepth.texture->texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, ourDepth.previousState)
		};
		commandList->ResourceBarrier(2, barriersBack);
	}

	sceneMgr.water.prepareForRendering(commandList);

	pass.mrt->PrepareAsTarget(commandList, pass.targets, true, TransitionFlags::UseDepth);

	ShaderConstantsProvider constants(provider.params, transparent.visibility, *ctx.camera, *pass.mrt);
	transparent.queue->renderObjects(constants, commandList);

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

CompositorTask::RunType SceneRenderTask::getRunType(CompositorPass& pass) const
{
	if (pass.info.entry == "Editor" || pass.info.entry == "Debug" || pass.info.entry == "Forward")
		return RunType::SyncCommands;
	else
		return RunType::Generic;
}

void SceneRenderTask::updateVoxelsDebugView(SceneEntity& debugVoxel, Camera& camera)
{
	auto orientation = camera.getOrientation();
	auto pos = camera.getPosition() - orientation * Vector3(0, 5.f, 0) + camera.getCameraDirection() * 1.75;

	debugVoxel.material = provider.resources.materials.getMaterial("VisualizeVoxelTexture");
	debugVoxel.geometry.fromModel(*provider.resources.models.getCoreModel("box.mesh"));
	debugVoxel.setTransformation({ orientation, pos, Vector3(10, 10, 1) }, true);
}
