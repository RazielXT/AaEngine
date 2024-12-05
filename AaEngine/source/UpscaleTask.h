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

	bool writesSyncCommands() const override { return true; }

private:
};
