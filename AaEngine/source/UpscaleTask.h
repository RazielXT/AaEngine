#pragma once

#include "CompositorTask.h"
#include "RenderQueue.h"
#include <thread>

class UpscaleTask : public CompositorTask
{
public:

	UpscaleTask(RenderProvider provider, SceneManager&);
	~UpscaleTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	RunType getRunType(CompositorPass&) const override { return RunType::SyncCommands; }

private:

	void prepare(RenderContext& ctx);
};
