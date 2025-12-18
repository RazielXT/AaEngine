#pragma once

#include "CompositorTask.h"
#include "AoPrepareDepthBuffersCS.h"
#include "AoRenderCS.h"
#include <thread>
#include "AoBlurAndUpsampleCS.h"

class ShadowMaps;

class SsaoComputeTask : public CompositorTask
{
public:

	SsaoComputeTask(RenderProvider provider, SceneManager&);
	~SsaoComputeTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;

	void runCompute(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncComputeCommands(CompositorPass&) const override
	{
		return true;
	}

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
