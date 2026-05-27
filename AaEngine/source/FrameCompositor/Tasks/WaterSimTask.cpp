#include "FrameCompositor/Tasks/WaterSimTask.h"
#include "Scene/RenderWorld.h"
#include "Scene/Camera.h"

WaterSimTask::WaterSimTask(RenderProvider provider, RenderWorld& w) : CompositorTask(provider, w)
{
}

WaterSimTask::~WaterSimTask()
{
}

AsyncTasksInfo WaterSimTask::initialize(CompositorPass& pass)
{
	return {};
}

void WaterSimTask::run(RenderContext& ctx, CommandsData& cmd, CompositorPass& pass)
{
	renderWorld.water.update(provider.renderSystem, cmd.commandList, provider.params.timeDelta, provider.params.frameIndex, ctx.camera->getPosition());
}

void WaterSimTask::runCompute(RenderContext& ctx, CommandsData& cmd, CompositorPass& pass)
{
	renderWorld.water.updateCompute(provider.renderSystem, cmd.commandList, provider.params.timeDelta, provider.params.frameIndex);
}

CompositorTask::RunType WaterSimTask::getRunType(CompositorPass& pass) const
{
	if (pass.info.entry == "Compute")
		return RunType::SyncComputeCommands;

	// PrepareFrame
	return RunType::SyncCommands;
}
