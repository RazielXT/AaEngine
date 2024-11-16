#pragma once

#include "CompositorTask.h"
#include "ScreenQuad.h"

class AaMaterial;

class DebugOverlayTask : public CompositorTask
{
public:

	DebugOverlayTask(RenderProvider, SceneManager&);
	~DebugOverlayTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands() const override;

private:

	AaMaterial* material{};
	ScreenQuad quad;
};
