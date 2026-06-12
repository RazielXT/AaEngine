#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"

class WaterSimTask : public CompositorTask
{
public:

	WaterSimTask(RenderProvider provider, RenderWorld&);
	~WaterSimTask();

	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override;

private:

};
