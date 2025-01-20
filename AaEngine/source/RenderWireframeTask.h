#pragma once

#include "CompositorTask.h"
#include "ScreenQuad.h"

class MaterialInstance;

class RenderWireframeTask : public CompositorTask
{
public:

	RenderWireframeTask(RenderProvider&, SceneManager&);
	~RenderWireframeTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands(CompositorPass&) const override
	{
		return true;
	}

	static RenderWireframeTask& Get();

	bool enabled = false;

	MaterialInstance* wireframeMaterial{};
};
