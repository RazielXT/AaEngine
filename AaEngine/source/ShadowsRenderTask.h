#pragma once

#include "CompositorTask.h"
#include "RenderQueue.h"
#include <thread>

class ShadowMaps;

class ShadowsRenderTask : public CompositorTask
{
public:

	ShadowsRenderTask(RenderProvider provider, SceneManager&, ShadowMaps& shadows);
	~ShadowsRenderTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

private:

	struct ShadowWork
	{
		CommandsData commands;
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;

		RenderObjectsVisibilityData renderablesData;
		RenderObjectsStorage* renderables;
	};

	ShadowWork cascades[3];
	ShadowWork maxShadow;

	bool running = true;

	RenderQueue* depthQueue{};

	ShadowMaps& shadowMaps;
};
