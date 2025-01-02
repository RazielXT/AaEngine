#pragma once

#include "CompositorTask.h"
#include "RenderQueue.h"
#include <thread>

class UpscalePrepareTask : public CompositorTask
{
public:

	UpscalePrepareTask(RenderProvider, SceneManager&);
	~UpscalePrepareTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands(CompositorPass&) const override { return true; }

private:
};

class UpscaleTask : public CompositorTask
{
public:

	UpscaleTask(RenderProvider provider, SceneManager&);
	~UpscaleTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands(CompositorPass&) const override { return true; }

private:
};
