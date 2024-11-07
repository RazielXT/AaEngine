#pragma once

#include "CompositorTask.h"
#include "RenderQueue.h"
#include <thread>

class AaShadowMap;
class AaSceneManager;

class ShadowsRenderTask : public CompositorTask
{
public:

	ShadowsRenderTask(RenderProvider provider, AaSceneManager&, AaShadowMap& shadows);
	~ShadowsRenderTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

private:

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

	RenderQueue* depthQueue{};

	AaShadowMap& shadowMaps;
};
