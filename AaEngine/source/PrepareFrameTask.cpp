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

	sceneMgr.water.update(cmd.commandList, provider.params.timeDelta, provider.params.frameIndex);

// 	sceneMgr.terrain.update(cmd.commandList, sceneMgr, ctx.camera->getPosition());
// 
// 	static int c = 0;
// 	if (c == 0)
// 	{
// 		CommandsMarker marker(cmd.commandList, "GrassCreation", PixColor::Foliage);
// 		for (UINT x = 1; x < 3; x++)
// 			for (UINT y = 1; y < 3; y++)
// 			{
// 				GrassAreaPlacementTask task;
// 				task.terrain = sceneMgr.terrain.grid.lods[0].data[x][y].entity;
// 				task.bbox = task.terrain->getWorldBoundingBox();
// 				sceneMgr.grass.scheduleGrassCreation(task, cmd.commandList, provider.params, provider.resources, sceneMgr);
// 			}
// 	}
// 	else if (c == 2)
// 	{
// 		CommandsMarker marker(cmd.commandList, "GrassCreation", PixColor::Foliage);
// 		sceneMgr.grass.finishGrassCreation();
// 	}
// 
// 	c++;
}
