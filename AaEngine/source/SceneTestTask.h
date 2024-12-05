#pragma once

#include "CompositorTask.h"
#include "SceneManager.h"
#include <thread>

class SceneTestTask : public CompositorTask
{
public:

	SceneTestTask(RenderProvider provider, SceneManager&);
	~SceneTestTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands() const override;

private:

	RenderTargetHeap heap;
	RenderTargetTexture tmp;
	RenderQueue tmpQueue;
};
