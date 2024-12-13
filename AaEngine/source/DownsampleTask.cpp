#include "DownsampleTask.h"

DownsampleDepthTask::DownsampleDepthTask(RenderProvider provider, SceneManager& s) : CompositorTask(provider, s)
{
}

DownsampleDepthTask::~DownsampleDepthTask()
{
}

AsyncTasksInfo DownsampleDepthTask::initialize(CompositorPass& pass)
{
	cs.init(provider.renderSystem->device, pass.info.name, ShaderRef{ "DepthDownsampleCS.hlsl", "main", "cs_6_6" });

	return {};
}

void DownsampleDepthTask::resize(CompositorPass& pass)
{
	cs.data.depthInputIdx = pass.inputs[0].texture->view.srvHeapIndex;
	cs.data.depthOutputIdx = pass.inputs[1].texture->view.uavHeapIndex;
	cs.data.depthOutputIdx2 = pass.inputs[2].texture->view.uavHeapIndex;
	cs.inputSize = { pass.inputs[0].texture->width, pass.inputs[0].texture->height };

	inputs = pass.inputs;
}

void DownsampleDepthTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	RenderTargetTransitions<3> tr;
	tr.addConst(inputs[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.addConst(inputs[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tr.addConst(inputs[2], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tr.push(syncCommands.commandList);

	cs.dispatch(syncCommands.commandList);
}

void DownsampleDepthCS::dispatch(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	ID3D12DescriptorHeap* ppHeaps[] = { DescriptorManager::get().mainDescriptorHeap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);

	const uint32_t bufferWidth = (inputSize.x + 15) / 16;
	const uint32_t bufferHeight = (inputSize.y + 15) / 16;

	commandList->Dispatch(bufferWidth, bufferHeight, 1);
}
