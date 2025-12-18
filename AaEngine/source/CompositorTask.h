#pragma once

#include "RenderContext.h"
#include "CompositorFileParser.h"

class SceneManager;

struct CompositorPass
{
	CompositorPassInfo& info;

	std::vector<GpuTextureStates> inputs;
	std::vector<GpuTextureStates> targets;

	std::unique_ptr<RenderTargetTexturesView> mrt;
	bool present = false;
	bool backbuffer = false;
};

class CompositorTask
{
public:

	CompositorTask(RenderProvider& p, SceneManager& s) : provider(p), sceneMgr(s) {};
	virtual ~CompositorTask() = default;

	virtual AsyncTasksInfo initialize(CompositorPass& pass) = 0;
	virtual void resize(CompositorPass& pass) {};

	virtual void run(RenderContext& ctx, CompositorPass& pass) {};

	virtual bool writesSyncCommands(CompositorPass&) const { return false; }
	virtual void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) {};

	virtual bool writesSyncComputeCommands(CompositorPass&) const { return false; }
	virtual void runCompute(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) {};

	virtual bool forceTaskOrder() const { return false; }

protected:

	RenderProvider provider;
	SceneManager& sceneMgr;
};
