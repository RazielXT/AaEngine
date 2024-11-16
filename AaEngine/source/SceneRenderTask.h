#pragma once

#include "AaRenderSystem.h"
#include "RenderContext.h"
#include "CompositorTask.h"
#include "RenderObject.h"
#include <thread>

struct RenderQueue;
class SceneManager;

class SceneRenderTask : public CompositorTask
{
public:

	SceneRenderTask(RenderProvider provider, SceneManager&);
	~SceneRenderTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	AsyncTasksInfo initializeEarlyZ(CompositorPass& pass);
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool forceTaskOrder() const override { return true; }

private:

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
	RenderObjectsVisibilityData sceneInfo;
	RenderObjectsStorage* renderables;

	void renderScene(CompositorPass& pass);
	void renderEarlyZ(CompositorPass& pass);

	RenderQueue* depthQueue{};
	RenderQueue* sceneQueue{};
};

class SceneRenderTransparentTask : public CompositorTask
{
public:

	SceneRenderTransparentTask(RenderProvider provider, SceneManager&);
	~SceneRenderTransparentTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

private:

	struct Work
	{
		CommandsData commands;
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;
	};
	Work transparent;

	bool running = true;

	RenderContext ctx;
	RenderObjectsVisibilityData sceneInfo;
	RenderObjectsStorage* renderables;

	void renderTransparentScene(CompositorPass& pass);

	RenderQueue* transparentQueue{};
};
