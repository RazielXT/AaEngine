#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "RenderCore/ScreenQuad.h"
#include "../PhysicsManager.h"

class PhysicsRenderTask : public CompositorTask
{
public:

	PhysicsRenderTask(RenderProvider, RenderWorld&, PhysicsManager&);
	~PhysicsRenderTask();

	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	enum Mode{ Off, Wireframe, Solid };
	void setMode(Mode);

	Execution getExecution(CompositorPass&) const override { return { RecordMode::Inline, Queue::Graphics }; }

private:

	Mode mode = Off;
	PhysicsManager& physicsMgr;
};
