#include "FrameCompositor/Tasks/DeferredVrtComputeTask.h"
#include "Resources/Shader/ShaderLibrary.h"
#include "Scene/Camera.h"

static constexpr UINT CascadesCount = 4;
static constexpr UINT RayCount = 4;

DeferredVrtComputeTask::DeferredVrtComputeTask(RenderProvider provider, RenderWorld& renderWorld) : CompositorTask(provider, renderWorld)
{
}

void DeferredVrtComputeTask::initialize(CompositorPass& pass)
{
	auto& device = *provider.renderSystem.core.device;
	resetQueueCS.init(device, *provider.resources.shaders.getShader("deferredVRT_resetQueueCS", ShaderType::Compute, ShaderRef{ "postprocess/deferredVRT_resetQueueCS.hlsl", "main", "cs_6_6" }));
	resetAccumulationCS.init(device, *provider.resources.shaders.getShader("deferredVRT_resetAccumulationCS", ShaderType::Compute, ShaderRef{ "postprocess/deferredVRT_resetAccumulationCS.hlsl", "main", "cs_6_6" }));
	generateRaysCS.init(device, *provider.resources.shaders.getShader("deferredVRT_generateRaysCS", ShaderType::Compute, ShaderRef{ "postprocess/deferredVRT_generateRaysCS.hlsl", "main", "cs_6_6" }));
	coarseTraceRayCS.init(device, *provider.resources.shaders.getShader("deferredVRT_coarseTraceRayCS", ShaderType::Compute, ShaderRef{ "postprocess/deferredVRT_coarseTraceRayCS.hlsl", "main", "cs_6_6" }));
	traceRayCS.init(device, *provider.resources.shaders.getShader("deferredVRT_traceRayCS", ShaderType::Compute, ShaderRef{ "postprocess/deferredVRT_traceRayCS.hlsl", "main", "cs_6_6" }));
	collectRaysCS.init(device, *provider.resources.shaders.getShader("deferredVRT_collectRaysCS", ShaderType::Compute, ShaderRef{ "postprocess/deferredVRT_collectRaysCS.hlsl", "main", "cs_6_6" }));

	createDispatchCommandSignature();
	initializeResources(pass);
}

void DeferredVrtComputeTask::resize(CompositorPass& pass)
{
	initializeResources(pass);
}

void DeferredVrtComputeTask::initializeResources(CompositorPass& pass)
{
	normal = pass.inputs[0];
	depth = pass.inputs[1];
	output = pass.targets[0];

	provider.resources.descriptors.createTextureView(*normal.texture);
	provider.resources.descriptors.createTextureView(*depth.texture);
	provider.resources.descriptors.createUAVView(*output.texture);

	sceneVoxelInfo = provider.resources.shaderBuffers.GetCbufferResource("SceneVoxelInfo");

	maxRays = output.texture->width * output.texture->height;
	UINT rayQueueSize = maxRays * sizeof(RayData);
	UINT rayResultsSize = maxRays * sizeof(RayResult);
	UINT accumulatedResultsSize = maxRays * sizeof(AccumulatedResult);

	rayQueues[0] = provider.resources.shaderBuffers.CreateStructuredBuffer(rayQueueSize);
	rayQueues[0]->SetName(L"DeferredVRT RayQueue 0");
	rayQueues[1] = provider.resources.shaderBuffers.CreateStructuredBuffer(rayQueueSize);
	rayQueues[1]->SetName(L"DeferredVRT RayQueue 1");
	rayQueues[2] = provider.resources.shaderBuffers.CreateStructuredBuffer(rayQueueSize);
	rayQueues[2]->SetName(L"DeferredVRT RayQueue CoarseHit");

	queueState = provider.resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT) * 4);
	queueState->SetName(L"DeferredVRT QueueState");
	rayResults = provider.resources.shaderBuffers.CreateStructuredBuffer(rayResultsSize);
	rayResults->SetName(L"DeferredVRT RayResults");
	accumulatedResults = provider.resources.shaderBuffers.CreateStructuredBuffer(accumulatedResultsSize);
	accumulatedResults->SetName(L"DeferredVRT AccumulatedResults");

	dispatchArgs[0] = provider.resources.shaderBuffers.CreateStructuredBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS));
	dispatchArgs[0]->SetName(L"DeferredVRT DispatchArgs 0");
	dispatchArgs[1] = provider.resources.shaderBuffers.CreateStructuredBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS));
	dispatchArgs[1]->SetName(L"DeferredVRT DispatchArgs 1");
	dispatchArgs[2] = provider.resources.shaderBuffers.CreateStructuredBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS));
	dispatchArgs[2]->SetName(L"DeferredVRT DispatchArgs CoarseHit");

	buffersNeedInitialTransition = true;
}

void DeferredVrtComputeTask::createDispatchCommandSignature()
{
	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc{};
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{};
	commandSignatureDesc.pArgumentDescs = &argumentDesc;
	commandSignatureDesc.NumArgumentDescs = 1;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);

	provider.renderSystem.core.device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&dispatchCommandSignature));
	dispatchCommandSignature->SetName(L"DeferredVRT DispatchCommandSignature");
}

void DeferredVrtComputeTask::transitionBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	if (before == after)
		return;

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
	commandList->ResourceBarrier(1, &barrier);
}

void DeferredVrtComputeTask::uavBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource);
	commandList->ResourceBarrier(1, &barrier);
}

void DeferredVrtComputeTask::recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass)
{
	CommandsMarker marker(commands.commandList, "Vgi", PixColor::CompositorCompute);

	TextureTransitions<3> textureTransitions;

	for (auto& i : pass.inputs)
	{
		textureTransitions.add(i.texture, i.previousState, i.state);
	}
	textureTransitions.add(pass.targets[0].texture, pass.targets[0].previousState, pass.targets[0].state);

	textureTransitions.push(commands.commandList);

	if (buffersNeedInitialTransition)
	{
		transitionBuffer(commands.commandList, rayQueues[0].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionBuffer(commands.commandList, rayQueues[1].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionBuffer(commands.commandList, rayQueues[2].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionBuffer(commands.commandList, queueState.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionBuffer(commands.commandList, rayResults.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionBuffer(commands.commandList, accumulatedResults.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionBuffer(commands.commandList, dispatchArgs[0].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionBuffer(commands.commandList, dispatchArgs[1].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionBuffer(commands.commandList, dispatchArgs[2].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		buffersNeedInitialTransition = false;
	}

	Matrix invViewProjection;
	XMStoreFloat4x4(&invViewProjection, XMMatrixTranspose(XMMatrixInverse(nullptr, ctx.camera->getViewProjectionMatrix())));

	DeferredVrtGenerateRaysCS::DispatchParams generateParams{};
	generateParams.InvViewProjectionMatrix = invViewProjection;
	generateParams.CameraPosition = ctx.camera->getPosition();
	generateParams.Time = provider.params.time;
	generateParams.ViewportSize = { output.texture->width, output.texture->height };
	generateParams.TexIdNormal = normal.texture->view.srvHeapIndex;
	generateParams.TexIdDepth = depth.texture->view.srvHeapIndex;
	generateParams.RayCount = RayCount;
	generateParams.OutputQueueIndex = 0;

	DeferredVrtResetAccumulationCS::DispatchParams resetAccumulationParams{};
	resetAccumulationParams.ViewportSize = { output.texture->width, output.texture->height };
	resetAccumulationCS.dispatch(commands.commandList, resetAccumulationParams, accumulatedResults.Get());
	uavBarrier(commands.commandList, accumulatedResults.Get());

	for (UINT rayIndex = 0; rayIndex < RayCount; rayIndex++)
	{
		marker.move(("VgiGenerateRay " + std::to_string(rayIndex)).c_str(), PixColor::CompositorCompute);

		generateParams.RayIndex = rayIndex;

		resetQueueCS.dispatch(commands.commandList, 0, queueState.Get(), dispatchArgs[0].Get());
		uavBarrier(commands.commandList, queueState.Get());
		uavBarrier(commands.commandList, dispatchArgs[0].Get());

		generateRaysCS.dispatch(commands.commandList, generateParams, rayQueues[0].Get(), rayResults.Get(), queueState.Get(), dispatchArgs[0].Get());
		uavBarrier(commands.commandList, rayQueues[0].Get());
		uavBarrier(commands.commandList, rayResults.Get());
		uavBarrier(commands.commandList, queueState.Get());
		uavBarrier(commands.commandList, dispatchArgs[0].Get());

		D3D12_RESOURCE_STATES queueStates[3] = { D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS };
		D3D12_RESOURCE_STATES argsStates[3] = { D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS };
		UINT inputIndex = 0;

		transitionBuffer(commands.commandList, rayQueues[0].Get(), queueStates[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		queueStates[0] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		transitionBuffer(commands.commandList, dispatchArgs[0].Get(), argsStates[0], D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		argsStates[0] = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

		for (UINT cascade = 0; cascade < CascadesCount; cascade++)
		{
			UINT outputIndex = 1 - inputIndex;
			UINT fineIndex = 2;

			transitionBuffer(commands.commandList, rayQueues[outputIndex].Get(), queueStates[outputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			queueStates[outputIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			transitionBuffer(commands.commandList, dispatchArgs[outputIndex].Get(), argsStates[outputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			argsStates[outputIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			resetQueueCS.dispatch(commands.commandList, outputIndex, queueState.Get(), dispatchArgs[outputIndex].Get());
			uavBarrier(commands.commandList, queueState.Get());
			uavBarrier(commands.commandList, dispatchArgs[outputIndex].Get());

			UINT traceInputIndex = inputIndex;
			if (cascade > 0)
			{
				transitionBuffer(commands.commandList, rayQueues[fineIndex].Get(), queueStates[fineIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				queueStates[fineIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				transitionBuffer(commands.commandList, dispatchArgs[fineIndex].Get(), argsStates[fineIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				argsStates[fineIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

				resetQueueCS.dispatch(commands.commandList, fineIndex, queueState.Get(), dispatchArgs[fineIndex].Get());
				uavBarrier(commands.commandList, queueState.Get());
				uavBarrier(commands.commandList, dispatchArgs[fineIndex].Get());

				DeferredVrtCoarseTraceRayCS::DispatchParams coarseParams{};
				coarseParams.InvViewProjectionMatrix = invViewProjection;
				coarseParams.CameraPosition = ctx.camera->getPosition();
				coarseParams.Time = provider.params.time;
				coarseParams.ViewportSize = { output.texture->width, output.texture->height };
				coarseParams.CascadeIndex = cascade;
				coarseParams.InputQueueIndex = inputIndex;
				coarseParams.HitQueueIndex = fineIndex;
				coarseParams.BypassQueueIndex = outputIndex;
				coarseParams.IsLastCascade = cascade == CascadesCount - 1;
				coarseParams.CoarseMip = 3;

				coarseTraceRayCS.dispatchIndirect(commands.commandList, dispatchCommandSignature.Get(), coarseParams, sceneVoxelInfo.data[provider.params.frameIndex]->GpuAddress(), rayQueues[inputIndex].Get(), rayQueues[fineIndex].Get(), rayQueues[outputIndex].Get(), rayResults.Get(), queueState.Get(), dispatchArgs[fineIndex].Get(), dispatchArgs[outputIndex].Get(), dispatchArgs[inputIndex].Get());
				uavBarrier(commands.commandList, rayQueues[fineIndex].Get());
				uavBarrier(commands.commandList, rayQueues[outputIndex].Get());
				uavBarrier(commands.commandList, rayResults.Get());
				uavBarrier(commands.commandList, queueState.Get());
				uavBarrier(commands.commandList, dispatchArgs[fineIndex].Get());
				uavBarrier(commands.commandList, dispatchArgs[outputIndex].Get());

				transitionBuffer(commands.commandList, dispatchArgs[inputIndex].Get(), argsStates[inputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				argsStates[inputIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				transitionBuffer(commands.commandList, rayQueues[inputIndex].Get(), queueStates[inputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				queueStates[inputIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

				transitionBuffer(commands.commandList, rayQueues[fineIndex].Get(), queueStates[fineIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				queueStates[fineIndex] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				transitionBuffer(commands.commandList, dispatchArgs[fineIndex].Get(), argsStates[fineIndex], D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
				argsStates[fineIndex] = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
				traceInputIndex = fineIndex;
			}

			DeferredVrtTraceRayCS::DispatchParams traceParams{};
			traceParams.InvViewProjectionMatrix = invViewProjection;
			traceParams.CameraPosition = ctx.camera->getPosition();
			traceParams.Time = provider.params.time;
			traceParams.ViewportSize = { output.texture->width, output.texture->height };
			traceParams.CascadeIndex = cascade;
			traceParams.InputQueueIndex = traceInputIndex;
			traceParams.OutputQueueIndex = outputIndex;
			traceParams.IsLastCascade = cascade == CascadesCount - 1;

			traceRayCS.dispatchIndirect(commands.commandList, dispatchCommandSignature.Get(), traceParams, sceneVoxelInfo.data[provider.params.frameIndex]->GpuAddress(), rayQueues[traceInputIndex].Get(), rayQueues[outputIndex].Get(), rayResults.Get(), queueState.Get(), dispatchArgs[outputIndex].Get(), dispatchArgs[traceInputIndex].Get());
			uavBarrier(commands.commandList, rayQueues[outputIndex].Get());
			uavBarrier(commands.commandList, rayResults.Get());
			uavBarrier(commands.commandList, queueState.Get());
			uavBarrier(commands.commandList, dispatchArgs[outputIndex].Get());

			transitionBuffer(commands.commandList, dispatchArgs[traceInputIndex].Get(), argsStates[traceInputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			argsStates[traceInputIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			transitionBuffer(commands.commandList, rayQueues[traceInputIndex].Get(), queueStates[traceInputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			queueStates[traceInputIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			if (cascade + 1 < CascadesCount)
			{
				transitionBuffer(commands.commandList, rayQueues[outputIndex].Get(), queueStates[outputIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				queueStates[outputIndex] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				transitionBuffer(commands.commandList, dispatchArgs[outputIndex].Get(), argsStates[outputIndex], D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
				argsStates[outputIndex] = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
				inputIndex = outputIndex;
			}
		}

		transitionBuffer(commands.commandList, rayResults.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		DeferredVrtCollectRaysCS::DispatchParams collectParams{};
		collectParams.ViewportSize = { output.texture->width, output.texture->height };
		collectParams.ResIdOutput = output.texture->view.uavHeapIndex;
		collectParams.RayCount = RayCount;
		collectParams.IsLastRay = rayIndex + 1 == RayCount;
		collectRaysCS.dispatch(commands.commandList, collectParams, rayResults.Get(), accumulatedResults.Get());
		uavBarrier(commands.commandList, accumulatedResults.Get());

		transitionBuffer(commands.commandList, rayResults.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
	marker.close();
}