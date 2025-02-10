#include "PrepareFrameTask.h"
#include "SceneManager.h"
#include "TerrainGenerator.h"
#include "Camera.h"

PrepareFrameTask::PrepareFrameTask(RenderProvider provider, SceneManager& s) : CompositorTask(provider, s)
{
}

PrepareFrameTask::~PrepareFrameTask()
{
}

AsyncTasksInfo PrepareFrameTask::initialize(CompositorPass& pass)
{
	return {};
}

void PrepareFrameTask::run(RenderContext& ctx, CommandsData& cmd, CompositorPass& pass)
{
	auto& dlss = provider.renderSystem.upscale.dlss;
	if (dlss.enabled())
		ctx.camera->setPixelOffset(dlss.getJitter(), dlss.getRenderSize());
	auto& fsr = provider.renderSystem.upscale.fsr;
	if (fsr.enabled())
		ctx.camera->setPixelOffset(fsr.getJitter(), fsr.getRenderSize());

	sceneMgr.terrain.update(cmd.commandList, sceneMgr, ctx.camera->getPosition());

	static int c = 0;
}
