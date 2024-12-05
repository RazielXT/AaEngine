#pragma once

#include "CompositorTask.h"
#include "RenderQueue.h"
#include <thread>

class AaShadowMap;

class ShadowsRenderTask : public CompositorTask
{
public:

	ShadowsRenderTask(RenderProvider provider, SceneManager&, AaShadowMap& shadows);
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

		RenderObjectsVisibilityData renderablesData;
		RenderObjectsStorage* renderables;
	}
	shadowsData[2];

	bool running = true;

	RenderQueue* depthQueue{};

	AaShadowMap& shadowMaps;
};
