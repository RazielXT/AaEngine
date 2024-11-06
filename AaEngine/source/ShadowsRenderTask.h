#pragma once

#include "CompositorTask.h"
#include "RenderQueue.h"
#include <thread>

class AaShadowMap;
class AaSceneManager;

class ShadowsRenderTask : public CompositorTask
{
public:

	ShadowsRenderTask(RenderProvider provider, AaShadowMap& shadows);
	~ShadowsRenderTask();

	AsyncTasksInfo initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target) override;
	void run(RenderContext& ctx, CommandsData& syncCommands) override;

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
