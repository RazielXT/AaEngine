#pragma once

#include "CompositorTask.h"
#include "AoPrepareDepthBuffersCS.h"
#include "AoRenderCS.h"
#include <thread>
#include "AoBlurAndUpsampleCS.h"

class AaShadowMap;

class SsaoComputeTask : public CompositorTask
{
public:

	SsaoComputeTask(RenderProvider provider, SceneManager&);
	~SsaoComputeTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;

	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

private:

	CommandsData commands;
	HANDLE eventBegin{};
	HANDLE eventFinish{};
	std::thread worker;

	bool running = true;

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
		RenderTargetTextureState sceneDepth;
		RenderTargetTextureState linearDepth;
		RenderTargetTextureState linearDepthDownsample2;
		RenderTargetTextureState linearDepthDownsampleAtlas2;
		RenderTargetTextureState linearDepthDownsample4;
		RenderTargetTextureState linearDepthDownsampleAtlas4;
		RenderTargetTextureState linearDepthDownsample8;
		RenderTargetTextureState linearDepthDownsampleAtlas8;
		RenderTargetTextureState linearDepthDownsample16;
		RenderTargetTextureState linearDepthDownsampleAtlas16;

		RenderTargetTextureState occlusionInterleaved8;
		RenderTargetTextureState occlusion8;
		RenderTargetTextureState occlusionInterleaved4;
		RenderTargetTextureState occlusion4;
		RenderTargetTextureState occlusionInterleaved2;

		RenderTargetTextureState aoSmooth4;
		RenderTargetTextureState aoSmooth2;
		RenderTargetTextureState aoSmooth;
	}
	textures;

	void loadTextures(CompositorPass& pass);
};
