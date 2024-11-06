#pragma once

#include "CompositorTask.h"
#include "ScreenQuad.h"

class AaMaterial;

class DebugOverlayTask : CompositorTask
{
public:

	DebugOverlayTask(RenderProvider);
	~DebugOverlayTask();

	AsyncTasksInfo initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target) override;
	void resize(RenderTargetTexture* target) override;
	void run(RenderContext& ctx, CommandsData& syncCommands) override;

private:

	RenderProvider provider;
	AaMaterial* material{};
	ScreenQuad quad;
};
