#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"

class AssignedMaterial;

class ImguiDebugWindowTask : public CompositorTask
{
public:

	ImguiDebugWindowTask(RenderProvider, RenderWorld&);
	~ImguiDebugWindowTask();

	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override { return { RecordMode::Inline, Queue::Graphics }; };
};
