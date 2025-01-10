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
		ShaderConstantsProvider constants(provider.params, {}, *ctx.camera, *pass.target.texture);

		pass.target.textureSet->PrepareAsTarget(syncCommands.commandList, false, TransitionFlags::DepthContinue);

		physicsMgr.drawDebugRender(syncCommands.commandList, &constants, pass.target.textureSet->formats, mode == Wireframe);
	}
}

bool PhysicsRenderTask::writesSyncCommands(CompositorPass&) const
{
	return true;
}

void PhysicsRenderTask::setMode(Mode m)
{
	mode = m;

	if (mode != Off)
		physicsMgr.enableRenderer(provider.renderSystem, provider.resources);
	else
		physicsMgr.disableRenderer();
}
