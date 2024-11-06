#pragma once

#include "CompositorTask.h"
#include "AaSceneManager.h"
#include <thread>

struct RenderQueue;
class AaSceneManager;

class SceneTestTask : public CompositorTask
{
public:

	SceneTestTask(RenderProvider provider);
	~SceneTestTask();

	AsyncTasksInfo initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target) override;
	void run(RenderContext& ctx, CommandsData& syncCommands) override;

private:

	RenderTargetHeap heap;
	RenderTargetTexture tmp;
	RenderQueue tmpQueue;

	AaSceneManager* sceneMgr;
	RenderProvider provider;
};
