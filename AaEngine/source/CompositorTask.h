#pragma once

#include "RenderContext.h"

class AaSceneManager;

struct PassInput
{
	ShaderTextureView* view{};
	RenderTargetTexture* texture{};
	D3D12_RESOURCE_STATES previousState{};
};
struct PassTarget
{
	RenderTargetTexture* texture{};
	D3D12_RESOURCE_STATES previousState{};
	bool present = false;
};
struct PassResources
{
	std::vector<PassInput> inputs;
	PassTarget target;
};

class CompositorTask
{
public:

	CompositorTask() = default;
	~CompositorTask() = default;

	virtual AsyncTasksInfo initialize(AaSceneManager* sceneMgr, RenderTargetTexture* target) = 0;
	virtual void resize(RenderTargetTexture* target) {};
	virtual void run(RenderContext& ctx, CommandsData& syncCommands) = 0;
};
