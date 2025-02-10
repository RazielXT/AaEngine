#pragma once

#include "CompositorTask.h"

class PrepareFrameTask : public CompositorTask
{
public:

	PrepareFrameTask(RenderProvider provider, SceneManager&);
	~PrepareFrameTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands(CompositorPass&) const override { return true; }

};
