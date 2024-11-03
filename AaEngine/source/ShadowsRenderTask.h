#pragma once

#include "RenderContext.h"
#include "RenderQueue.h"
#include <thread>

class AaShadowMap;
class AaSceneManager;

class ShadowsRenderTask
{
public:

	ShadowsRenderTask(RenderProvider provider, AaShadowMap& shadows);
	~ShadowsRenderTask();

	AsyncTasksInfo initialize(AaRenderSystem* renderSystem, AaSceneManager* sceneMgr);
	void prepare(RenderContext& ctx, CommandsData& syncCommands);

	struct 
	{
		CommandsData commands;
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;

		RenderInformation renderablesData;
	}
	shadowsData[2];

	bool running = true;

	RenderContext ctx;

	RenderQueue* depthQueue;

	AaShadowMap& shadowMaps;

	RenderProvider provider;
};
