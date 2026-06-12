#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "Scene/RenderQueue.h"
#include <thread>

class UpscaleTask : public CompositorTask
{
public:

	UpscaleTask(RenderProvider provider, RenderWorld&);
	~UpscaleTask();

	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override { return { RecordMode::Inline, Queue::Graphics }; }

private:

	void prepare(RenderContext& ctx);
};
