#pragma once

#include "CompositorTask.h"
#include "AoPrepareDepthBuffersCS.h"
#include "AoRenderCS.h"
#include <thread>
#include "AoBlurAndUpsampleCS.h"

class AaShadowMap;
class SceneManager;

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

	struct
	{
		RenderTargetTexture* sceneDepth;
		RenderTargetTexture* linearDepth;
		RenderTargetTexture* linearDepthDownsample2;
		RenderTargetTexture* linearDepthDownsampleAtlas2;
		RenderTargetTexture* linearDepthDownsample4;
		RenderTargetTexture* linearDepthDownsampleAtlas4;
		RenderTargetTexture* linearDepthDownsample8;
		RenderTargetTexture* linearDepthDownsampleAtlas8;
		RenderTargetTexture* linearDepthDownsample16;
		RenderTargetTexture* linearDepthDownsampleAtlas16;

		RenderTargetTexture* occlusionInterleaved8;
		RenderTargetTexture* occlusion8;
		RenderTargetTexture* occlusionInterleaved4;
		RenderTargetTexture* occlusion4;
		RenderTargetTexture* occlusionInterleaved2;

		RenderTargetTexture* aoSmooth4;
		RenderTargetTexture* aoSmooth2;
		RenderTargetTexture* aoSmooth;
	}
	textures;

	void loadTextures(CompositorPass& pass);
};
