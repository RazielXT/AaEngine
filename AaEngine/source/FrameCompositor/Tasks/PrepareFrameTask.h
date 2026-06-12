#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"

class PrepareFrameTask : public CompositorTask
{
public:

	PrepareFrameTask(RenderProvider provider, RenderWorld&);
	~PrepareFrameTask();

	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override;

private:

	void prepareMotionVectors(RenderContext& ctx);

};
