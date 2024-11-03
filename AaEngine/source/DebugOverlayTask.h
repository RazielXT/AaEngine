#pragma once

#include "RenderContext.h"
#include "ScreenQuad.h"

class AaMaterial;

class DebugOverlayTask
{
public:

	DebugOverlayTask(RenderProvider);
	~DebugOverlayTask();

	AsyncTasksInfo initialize(RenderTargetTexture* target);

	void resize(RenderTargetTexture* target);
	void prepare(RenderContext& ctx, CommandsData& syncCommands);

private:

	RenderProvider provider;
	AaMaterial* material{};
	ScreenQuad quad;
};
