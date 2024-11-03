#pragma once

#include "AaRenderSystem.h"
#include "RenderContext.h"
#include "AaSceneManager.h"
#include <thread>

struct RenderQueue;
class AaSceneManager;

class SceneTestTask
{
public:

	SceneTestTask(RenderProvider provider);
	~SceneTestTask();

	AsyncTasksInfo initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target);
	void prepare(RenderContext& ctx, CommandsData& syncCommands);
	void finish();

private:

	RenderTargetHeap heap;
	RenderTargetTexture tmp;
	RenderQueue tmpQueue;

	AaSceneManager* sceneMgr;
	RenderProvider provider;
};
