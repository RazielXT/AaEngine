#include "PhysicsRenderTask.h"
#include "ShaderConstantsProvider.h"
#include "RenderObject.h"

PhysicsRenderTask::PhysicsRenderTask(RenderProvider p, SceneManager& s, PhysicsManager& m) : CompositorTask(p, s), physicsMgr(m)
{
}

PhysicsRenderTask::~PhysicsRenderTask()
{
}

AsyncTasksInfo PhysicsRenderTask::initialize(CompositorPass& pass)
{
	return {};
}

void PhysicsRenderTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	if (mode != Off)
	{
		ShaderConstantsProvider constants(provider.params, {}, *ctx.camera, *pass.mrt);

		pass.mrt->PrepareAsTarget(syncCommands.commandList, pass.targets, false, TransitionFlags::DepthContinue);

		physicsMgr.drawDebugRender(syncCommands.commandList, &constants, pass.mrt->formats, mode == Wireframe);
	}
}

void PhysicsRenderTask::setMode(Mode m)
{
	mode = m;

	if (mode != Off)
		physicsMgr.enableRenderer(provider.renderSystem, provider.resources);
	else
		physicsMgr.disableRenderer(provider.renderSystem);
}
