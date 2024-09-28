#pragma once

#include "AaRenderSystem.h"
#include "RenderContext.h"
#include "AaSceneManager.h"
#include <thread>

struct RenderQueue;

class SceneRenderTask
{
public:

	SceneRenderTask();
	~SceneRenderTask();

	AsyncTasksInfo initialize(AaRenderSystem* renderSystem, RenderQueue* queue);
	AsyncTasksInfo initializeEarlyZ(AaRenderSystem* renderSystem, RenderQueue* queue);
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

	RenderContext ctx;
	RenderInformation sceneInfo;

private:

	void renderScene();
	void renderEarlyZ();

	RenderQueue* depthQueue;
	RenderQueue* sceneQueue;
};
