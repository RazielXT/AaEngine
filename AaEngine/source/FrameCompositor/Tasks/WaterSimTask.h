#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"

class WaterSimTask : public CompositorTask
{
public:

	WaterSimTask(RenderProvider provider, RenderWorld&);
	~WaterSimTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;
	void runCompute(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	RunType getRunType(CompositorPass&) const override;

private:

};
