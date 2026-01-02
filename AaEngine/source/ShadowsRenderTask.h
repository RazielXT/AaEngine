#pragma once

#include "CompositorTask.h"
#include "RenderQueue.h"
#include "ShadowMaps.h"
#include <thread>

class ShadowsRenderTask : public CompositorTask
{
public:

	ShadowsRenderTask(RenderProvider provider, SceneManager&, ShadowMaps& shadows);
	~ShadowsRenderTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CompositorPass& pass) override;

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

	ShadowWork cascades[4];

	void prepareShadowCascade(ShadowWork& work, ShadowMaps::ShadowData& data);

	bool running = true;

	RenderQueue* depthQueue{};

	ShadowMaps& shadowMaps;

	RenderContext ctx;
};
