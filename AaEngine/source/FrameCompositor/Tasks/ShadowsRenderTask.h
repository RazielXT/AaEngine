#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "Scene/RenderQueue.h"
#include "RenderCore/ShadowMaps.h"
#include <thread>

class ShadowsRenderTask : public CompositorTask
{
public:

	ShadowsRenderTask(RenderProvider provider, RenderWorld&, ShadowMaps& shadows);
	~ShadowsRenderTask();

	AsyncTasksInfo buildAsyncTasks(CompositorPass& pass) override;
	void update(RenderContext& ctx, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override { return { RecordMode::Threaded, Queue::Graphics }; }

private:

	struct ShadowWork
	{
		CommandsData commands;
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;

		RenderObjectsVisibilityData renderablesData;
		std::vector<UINT> idFilter;

		RenderObjectsStorage* renderables;
		uint8_t filterFlag{};

		RenderViewId viewId{};
	};

	ShadowWork cascades[4];

	void prepareShadowCascade(ShadowWork& work, ShadowMaps::ShadowData& data);

	bool running = true;

	RenderQueue* depthQueue{};

	ShadowMaps& shadowMaps;

	RenderContext ctx;
};
