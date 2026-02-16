#pragma once

#include "CompositorTask.h"

class AssignedMaterial;

class ImguiDebugWindowTask : public CompositorTask
{
public:

	ImguiDebugWindowTask(RenderProvider, SceneManager&);
	~ImguiDebugWindowTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	RunType getRunType(CompositorPass&) const override { return RunType::SyncCommands; };
};
