#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "Resources/Compute/AoPrepareDepthBuffersCS.h"
#include "Resources/Compute/AoRenderCS.h"
#include <thread>
#include "Resources/Compute/AoBlurAndUpsampleCS.h"

class ShadowMaps;

class SsaoComputeTask : public CompositorTask
{
public:

	SsaoComputeTask(RenderProvider provider, RenderWorld&);
	~SsaoComputeTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;

	void runCompute(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	RunType getRunType(CompositorPass&) const override { return RunType::SyncComputeCommands; }

private:

	AoPrepareDepthBuffersCS prepareDepthBuffersCS;
	AoPrepareDepthBuffers2CS prepareDepthBuffers2CS;
	AoRenderCS aoRenderCS;
	AoRenderCS aoRenderInterleaveCS;
	AoBlurAndUpsampleCS aoBlurAndUpsampleCS;
	AoBlurAndUpsampleCS aoBlurAndUpsampleFinalCS;

	float TanHalfFovH{};

	void initializeResources(CompositorPass& pass);

	struct Txt
	{
		TextureStatePair sceneDepth;
		TextureStatePair linearDepth;
		TextureStatePair linearDepthDownsample2;
		TextureStatePair linearDepthDownsampleAtlas2;
		TextureStatePair linearDepthDownsample4;
		TextureStatePair linearDepthDownsampleAtlas4;
		TextureStatePair linearDepthDownsample8;
		TextureStatePair linearDepthDownsampleAtlas8;
		TextureStatePair linearDepthDownsample16;
		TextureStatePair linearDepthDownsampleAtlas16;

		TextureStatePair occlusionInterleaved8;
		TextureStatePair occlusion8;
		TextureStatePair occlusionInterleaved4;
		TextureStatePair occlusion4;
		TextureStatePair occlusionInterleaved2;

		TextureStatePair aoSmooth4;
		TextureStatePair aoSmooth2;
		TextureStatePair aoSmooth;
	}
	textures;

	void loadTextures(CompositorPass& pass);
};
