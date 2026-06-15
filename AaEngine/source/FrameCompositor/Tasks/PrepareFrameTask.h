#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"

class ShadowMaps;

class PrepareFrameTask : public CompositorTask
{
public:

	PrepareFrameTask(RenderProvider provider, RenderWorld&, ShadowMaps& shadows);
	~PrepareFrameTask();

	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override;

private:

	void prepareMotionVectors(RenderContext& ctx);

	ShadowMaps& shadowMaps;
};
