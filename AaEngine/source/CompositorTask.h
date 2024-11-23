#pragma once

#include "RenderContext.h"
#include "CompositorFileParser.h"

class SceneManager;

struct PassInput : public RenderTargetTextureState
{
};
struct PassTarget : public RenderTargetTextureState
{
	bool present = false;
	bool backbuffer = false;
};
struct CompositorPass
{
	CompositorPassInfo& info;

	std::vector<PassInput> inputs;
	PassTarget target;
};

class CompositorTask
{
public:

	CompositorTask(RenderProvider& p, SceneManager& s) : provider(p), sceneMgr(s) {};
	virtual ~CompositorTask() = default;

	virtual AsyncTasksInfo initialize(CompositorPass& pass) = 0;
	virtual void resize(CompositorPass& pass) {};
	virtual void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) = 0;

	virtual bool writesSyncCommands() const { return false; }
	virtual bool forceTaskOrder() const { return false; }

protected:

	RenderProvider provider;
	SceneManager& sceneMgr;
};
