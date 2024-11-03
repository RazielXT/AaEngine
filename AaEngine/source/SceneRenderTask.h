#pragma once

#include "AaRenderSystem.h"
#include "RenderContext.h"
#include "AaSceneManager.h"
#include <thread>

struct RenderQueue;
class AaSceneManager;

class SceneRenderTask
{
public:

	SceneRenderTask(RenderProvider provider);
	~SceneRenderTask();

	AsyncTasksInfo initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target);
	AsyncTasksInfo initializeEarlyZ(AaSceneManager* sceneMgr);
	void prepare(RenderContext& ctx, CommandsData& syncCommands);

	struct Work
	{
		CommandsData commands;
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;
	};
	Work earlyZ;
	Work scene;

	bool running = true;

	RenderProvider provider;
	RenderContext ctx;
	RenderInformation sceneInfo;

private:

	void renderScene();
	void renderEarlyZ();

	RenderQueue* depthQueue;
	RenderQueue* sceneQueue;
};
