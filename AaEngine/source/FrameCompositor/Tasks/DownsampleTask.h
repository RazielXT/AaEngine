#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "Scene/RenderQueue.h"
#include "Resources/Compute/ComputeShader.h"

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

	DownsampleDepthTask(RenderProvider provider, RenderWorld&);
	~DownsampleDepthTask();

	void initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;
	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override { return { RecordMode::Inline, Queue::Graphics }; }

private:

	std::vector<GpuTextureStates> inputs;
	DownsampleDepthCS cs;
};
