#pragma once

#include "CompositorTask.h"
#include "RenderQueue.h"
#include "ComputeShader.h"

class DownsampleDepthCS : public ComputeShader
{
public:

	struct 
	{
		UINT depthInputIdx{};
		UINT depthOutputIdx{};
		UINT depthOutputIdx2{};
	}
	data;

	XMUINT2 inputSize{};

	void dispatch(ID3D12GraphicsCommandList* commandList);
};

class DownsampleDepthTask : public CompositorTask
{
public:

	DownsampleDepthTask(RenderProvider provider, SceneManager&);
	~DownsampleDepthTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands(CompositorPass&) const override { return true; }

private:

	std::vector<GpuTextureStates> inputs;
	DownsampleDepthCS cs;
};
