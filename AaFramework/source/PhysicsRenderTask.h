#pragma once

#include "CompositorTask.h"
#include "ScreenQuad.h"
#include "PhysicsManager.h"

class PhysicsRenderTask : public CompositorTask
{
public:

	PhysicsRenderTask(RenderProvider, SceneManager&, PhysicsManager&);
	~PhysicsRenderTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	enum Mode{ Off, Wireframe, Solid };
	void setMode(Mode);

	RunType getRunType(CompositorPass&) const override { return RunType::SyncCommands; }

private:

	Mode mode = Off;
	PhysicsManager& physicsMgr;
};
