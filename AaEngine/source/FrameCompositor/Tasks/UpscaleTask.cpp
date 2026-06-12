#include "FrameCompositor/Tasks/UpscaleTask.h"

UpscaleTask::UpscaleTask(RenderProvider provider, RenderWorld& w) : CompositorTask(provider, w)
{
}

UpscaleTask::~UpscaleTask()
{
}

void UpscaleTask::recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass)
{
	CommandsMarker marker(commands.commandList, "Upscale", PixColor::Upscale);

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
	barriers.resize(pass.inputs.size() + 1);
	int b = 0;

	for (auto& input : pass.inputs)
	{
		auto targetState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		if (!(input.previousState & targetState))
			barriers[b++] = CD3DX12_RESOURCE_BARRIER::Transition(
			input.texture->texture.Get(),
			input.previousState,
				targetState);
	}
	{
		auto& target = pass.targets.front();

		barriers[b++] = CD3DX12_RESOURCE_BARRIER::Transition(
			target.texture->texture.Get(),
			target.previousState,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	commands.commandList->ResourceBarrier(b, barriers.data());

	auto& dlss = provider.renderSystem.upscale.dlss;
	if (dlss.enabled())
	{
		DLSS::UpscaleInput input;
		input.unresolvedColorResource = pass.inputs[0].texture->texture.Get();
		input.motionVectorsResource = pass.inputs[1].texture->texture.Get();
		input.depthResource = pass.inputs[2].texture->texture.Get();
		input.resolvedColorResource = pass.targets.front().texture->texture.Get();
		input.tslf = provider.params.timeDelta;
		dlss.upscale(commands.commandList, input);
	}
	auto& fsr = provider.renderSystem.upscale.fsr;
	if (fsr.enabled())
	{
		FSR::UpscaleInput input;
		input.unresolvedColorResource = pass.inputs[0].texture->texture.Get();
		input.motionVectorsResource = pass.inputs[1].texture->texture.Get();
		input.depthResource = pass.inputs[2].texture->texture.Get();
		input.resolvedColorResource = pass.targets.front().texture->texture.Get();
		input.tslf = provider.params.timeDelta;
		fsr.upscale(commands.commandList, input, *ctx.camera);
	}

	//set them back if changed
	auto& descriptors = DescriptorManager::get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptors.mainDescriptorHeap, descriptors.samplerHeap };
	commands.commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}
