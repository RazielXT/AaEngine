#pragma once

#include "CompositorTask.h"

class AaMaterial;

class ImguiDebugWindowTask : public CompositorTask
{
public:

	ImguiDebugWindowTask(RenderProvider, AaSceneManager&);
	~ImguiDebugWindowTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands() const override;
};
