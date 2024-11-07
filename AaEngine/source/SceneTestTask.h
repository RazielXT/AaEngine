#pragma once

#include "CompositorTask.h"
#include "AaSceneManager.h"
#include <thread>

struct RenderQueue;
class AaSceneManager;

class SceneTestTask : public CompositorTask
{
public:

	SceneTestTask(RenderProvider provider, AaSceneManager&);
	~SceneTestTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool isSync() const override;

private:

	RenderTargetHeap heap;
	RenderTargetTexture tmp;
	RenderQueue tmpQueue;
};
