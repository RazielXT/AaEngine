#pragma once

#include "RenderCore/RenderSystem.h"
#include "FrameCompositor/RenderContext.h"
#include "FrameCompositor/Tasks/CompositorTask.h"
#include "Scene/RenderObject.h"
#include <thread>
#include "Editor/EntityPicker.h"
#include "RenderCore/ShadowMaps.h"

struct RenderQueue;

class SceneRenderTask : public CompositorTask
{
public:

	SceneRenderTask(RenderProvider provider, RenderWorld&, ShadowMaps&);
	~SceneRenderTask();

	AsyncTasksInfo buildAsyncTasks(CompositorPass& pass) override;

	void update(RenderContext& ctx, CompositorPass& pass) override;
	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	bool forceTaskOrder() const override { return true; }

	void resize(CompositorPass& pass) override;

	static SceneRenderTask& Get();

	void showVoxels(bool show);

	Execution getExecution(CompositorPass&) const override;

	void setViewOverride(RenderViewId view);

private:

	AsyncTasksInfo initializeEarlyZ(CompositorPass& pass);

	struct AsyncWork
	{
		CommandsData commands;
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;
	};

	bool running = true;

	RenderContext ctx;

	struct
	{
		AsyncWork work;
		RenderObjectsVisibilityData visibility;
		RenderObjectsStorage* renderables;
		RenderQueue* queue{};
		RenderQueue* wireframeQueue{};
	}
	opaque;

	struct
	{
		AsyncWork work;
		RenderQueue* queue{};
	}
	earlyZ;

	struct
	{
		RenderObjectsVisibilityData visibility;
		RenderObjectsStorage* renderables;
		RenderQueue* queue{};
		RenderQueue* wireframeQueue{};
	}
	forward;

	struct
	{
		AsyncWork work;
		RenderObjectsVisibilityData visibility;
		RenderObjectsStorage* renderables;
		RenderQueue* queue{};
		RenderQueue* wireframeQueue{};
	}
	transparent;

	void renderScene(CompositorPass& pass);
	void renderEarlyZ(CompositorPass& pass);
	void renderTransparentScene(CompositorPass& pass);
	void renderWireframe(CompositorPass& pass);
	void renderForward(CompositorPass& pass, CommandsData& cmd);

	EntityPicker picker;
	void renderEditor(CompositorPass& pass, CommandsData& cmd);
	void renderDebug(CompositorPass& pass, CommandsData& cmd);

	bool showVoxelsEnabled = false;
	void updateVoxelsDebugView(RenderEntity& debugVoxel, Camera& camera);

	const Camera& getViewCamera() const;
	RenderViewId viewOverride{ RenderViewId_Default };
	ShadowMaps& shadowMaps;
};
