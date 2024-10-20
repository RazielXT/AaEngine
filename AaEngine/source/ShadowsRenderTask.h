#pragma once

#include "RenderContext.h"
#include <thread>

class AaShadowMap;
class AaSceneManager;
struct RenderQueue;

class ShadowsRenderTask
{
public:

	ShadowsRenderTask(AaShadowMap& shadows);
	~ShadowsRenderTask();

	AsyncTasksInfo initialize(AaRenderSystem* renderSystem, AaSceneManager* sceneMgr);
	void prepare(RenderContext& ctx, CommandsData& syncCommands);

	struct 
	{
		CommandsData commands;
		HANDLE eventBegin;
		HANDLE eventFinish;
		std::thread worker;
	}
	shadowsData[2];

	bool running = true;

	RenderContext ctx;

	RenderQueue* depthQueue;

	AaShadowMap& shadowMaps;
};
