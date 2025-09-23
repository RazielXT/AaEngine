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
	CommandsMarker marker(syncCommands.commandList, "Upscale", PixColor::Upscale);

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

	auto& dlss = provider.renderSystem.upscale.dlss;
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
	auto& fsr = provider.renderSystem.upscale.fsr;
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

	//set them back if changed
	auto& descriptors = DescriptorManager::get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptors.mainDescriptorHeap, descriptors.samplerHeap };
	syncCommands.commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

UpscalePrepareTask::UpscalePrepareTask(RenderProvider provider, SceneManager& s) : CompositorTask(provider, s)
{
}

UpscalePrepareTask::~UpscalePrepareTask()
{
}

AsyncTasksInfo UpscalePrepareTask::initialize(CompositorPass& pass)
{
	return {};
}

void UpscalePrepareTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	// Assuming view and viewPrevious are pointers to View instances
	XMMATRIX viewMatrixCurrent = ctx.camera->getViewMatrix();
	static XMMATRIX viewMatrixPrevious = viewMatrixCurrent;
	XMMATRIX projMatrixCurrent = ctx.camera->getProjectionMatrixNoOffset();
	static XMMATRIX projMatrixPrevious = projMatrixCurrent;

	// Calculate the view reprojection matrix
	XMMATRIX viewReprojectionMatrix = XMMatrixMultiply(XMMatrixInverse(nullptr, viewMatrixCurrent), viewMatrixPrevious);

	// Calculate the reprojection matrix for TAA
	XMMATRIX reprojectionMatrix = XMMatrixMultiply(XMMatrixMultiply(XMMatrixInverse(nullptr, projMatrixCurrent), viewReprojectionMatrix), projMatrixPrevious);

	viewMatrixPrevious = viewMatrixCurrent;
	projMatrixPrevious = projMatrixCurrent;

	XMFLOAT4X4 reprojectionData;
	XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&reprojectionData, XMMatrixTranspose(reprojectionMatrix));
	provider.resources.materials.getMaterial("MotionVectors")->SetParameter("reprojectionMatrix", &reprojectionData._11, 16);
}
