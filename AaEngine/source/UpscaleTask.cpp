#include "UpscaleTask.h"

UpscaleTask::UpscaleTask(RenderProvider provider, SceneManager& s) : CompositorTask(provider, s)
{
}

UpscaleTask::~UpscaleTask()
{
}

AsyncTasksInfo UpscaleTask::initialize(CompositorPass& pass)
{
	return {};
}

void UpscaleTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	CommandsMarker marker(syncCommands.commandList, "Upscale");

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
	barriers.resize(pass.inputs.size());
	int b = 0;

	auto last = &pass.inputs.back();

	for (auto& input : pass.inputs)
	{
		auto targetState = &input == last ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		if (input.previousState != targetState)
			barriers[b++] = CD3DX12_RESOURCE_BARRIER::Transition(
			input.texture->texture.Get(),
			input.previousState,
				targetState);
	}

	syncCommands.commandList->ResourceBarrier(b, barriers.data());

	auto& dlss = provider.renderSystem->upscale.dlss;
	if (dlss.enabled())
	{
		DLSS::UpscaleInput input;
		input.unresolvedColorResource = pass.inputs[0].texture->texture.Get();
		input.motionVectorsResource = pass.inputs[1].texture->texture.Get();
		input.depthResource = pass.inputs[2].texture->texture.Get();
		input.resolvedColorResource = pass.inputs[3].texture->texture.Get();
		input.tslf = provider.params.timeDelta;
		dlss.upscale(syncCommands.commandList, input);
	}
	auto& fsr = provider.renderSystem->upscale.fsr;
	if (fsr.enabled())
	{
		FSR::UpscaleInput input;
		input.unresolvedColorResource = pass.inputs[0].texture->texture.Get();
		input.motionVectorsResource = pass.inputs[1].texture->texture.Get();
		input.depthResource = pass.inputs[2].texture->texture.Get();
		input.resolvedColorResource = pass.inputs[3].texture->texture.Get();
		input.tslf = provider.params.timeDelta;
		fsr.upscale(syncCommands.commandList, input, *ctx.camera);
	}
}
