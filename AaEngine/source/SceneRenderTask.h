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

	AsyncTasksInfo initializeEarlyZ(AaSceneManager* sceneMgr);
	AsyncTasksInfo initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target, const std::vector<std::string>& params);
	AsyncTasksInfo initializeTransparent(AaSceneManager* sceneMgr, RenderTargetTexture* target);

	void run(RenderContext& ctx, CommandsData& syncCommands);

	struct Work
	{
		CommandsData commands;
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;
	};
	Work earlyZ;
	Work scene;
	Work transparent;

	bool running = true;

	RenderProvider provider;
	RenderContext ctx;
	RenderInformation sceneInfo;

private:

	void renderScene();
	void renderTransparentScene();
	void renderEarlyZ();

	RenderQueue* depthQueue;
	RenderQueue* sceneQueue;
	RenderQueue* transparentQueue;
};
