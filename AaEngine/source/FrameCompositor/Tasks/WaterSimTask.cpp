#include "FrameCompositor/Tasks/WaterSimTask.h"
#include "Scene/RenderWorld.h"
#include "Scene/Camera.h"

WaterSimTask::WaterSimTask(RenderProvider provider, RenderWorld& w) : CompositorTask(provider, w)
{
}

WaterSimTask::~WaterSimTask()
{
}

void WaterSimTask::recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass)
{
	if (pass.info.entry == "Compute")
		renderWorld.water.updateCompute(provider.renderSystem, commands.commandList, provider.params.timeDelta, provider.params.frameIndex);
	else
		renderWorld.water.update(provider.renderSystem, commands.commandList, provider.params.timeDelta, provider.params.frameIndex, ctx.camera->getPosition());
}

CompositorTask::Execution WaterSimTask::getExecution(CompositorPass& pass) const
{
	if (pass.info.entry == "Compute")
		return { RecordMode::Inline, Queue::Compute };

	// PrepareFrame
	return { RecordMode::Inline, Queue::Graphics };
}
