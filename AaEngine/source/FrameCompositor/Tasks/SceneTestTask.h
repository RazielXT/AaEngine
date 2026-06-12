#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "Scene/RenderWorld.h"
#include <thread>

class SceneTestTask : public CompositorTask
{
public:

	SceneTestTask(RenderProvider provider, RenderWorld&);
	~SceneTestTask();

	void initialize(CompositorPass& pass) override;
	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override { return { RecordMode::Inline, Queue::Graphics }; }

private:

	RenderTargetHeap heap;
	RenderTargetTextures textures;
	RenderQueue tmpQueue;
};
