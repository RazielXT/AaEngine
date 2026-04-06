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

	enum class RunType { Generic, SyncCommands, SyncComputeCommands };
	virtual RunType getRunType(CompositorPass&) const = 0;

	virtual void run(RenderContext& ctx, CompositorPass& pass) {};
	virtual void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) {};
	virtual void runCompute(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) {};

	virtual bool forceTaskOrder() const { return false; }

protected:

	RenderProvider provider;
	SceneManager& sceneMgr;
};
