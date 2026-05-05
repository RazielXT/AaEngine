#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "Scene/RenderWorld.h"
#include <thread>

class SceneTestTask : public CompositorTask
{
public:

	SceneTestTask(RenderProvider provider, RenderWorld&);
	~SceneTestTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	RunType getRunType(CompositorPass&) const override { return RunType::SyncCommands; }

private:

	RenderTargetHeap heap;
	RenderTargetTextures textures;
	RenderQueue tmpQueue;
};
