#pragma once

#include "RenderSystem.h"
#include "RenderContext.h"
#include "CompositorTask.h"
#include "RenderObject.h"
#include <thread>
#include "EntityPicker.h"

struct RenderQueue;

class SceneRenderTask : public CompositorTask
{
public:

	SceneRenderTask(RenderProvider provider, SceneManager&);
	~SceneRenderTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool forceTaskOrder() const override { return true; }

	void resize(CompositorPass& pass) override;

	bool writesSyncCommands(CompositorPass&) const override;

	static SceneRenderTask& Get();

	bool enabledWireframe = false;

	void showVoxelsInfo(bool show);

private:

	AsyncTasksInfo initializeEarlyZ(CompositorPass& pass);

	struct AsyncWork
	{
		CommandsData commands;
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;
	};
	AsyncWork earlyZ;
	AsyncWork scene;

	bool running = true;

	RenderContext ctx;
	RenderObjectsVisibilityData sceneVisibility;
	RenderObjectsStorage* renderables;

	void renderScene(CompositorPass& pass);
	void renderEarlyZ(CompositorPass& pass);

	RenderQueue* depthQueue{};
	RenderQueue* sceneQueue{};

	struct 
	{
		RenderObjectsVisibilityData sceneVisibility;
		RenderObjectsStorage* renderables;
		RenderQueue* transparentQueue{};

		AsyncWork work;
	}
	transparent;

	void renderTransparentScene(CompositorPass& pass);

	EntityPicker picker;
	void renderEditor(CompositorPass& pass, CommandsData& cmd);

	RenderQueue* wireframeQueue{};
	void renderWireframe(CompositorPass& pass, CommandsData& cmd);

	void renderDebug(CompositorPass& pass, CommandsData& cmd);

	bool showVoxelsEnabled = false;
	void showVoxelsUpdate(SceneEntity& debugVoxel, Camera& camera);
	void showVoxels();
	void hideVoxels();
};
