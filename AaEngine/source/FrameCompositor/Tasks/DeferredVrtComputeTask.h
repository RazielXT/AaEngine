#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "Resources/Compute/DeferredVrtCS.h"
#include "Resources/Shader/ShaderDataBuffers.h"

class DeferredVrtComputeTask : public CompositorTask
{
public:
	DeferredVrtComputeTask(RenderProvider provider, RenderWorld& renderWorld);

	void initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;
	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	Execution getExecution(CompositorPass&) const override { return { RecordMode::Inline, Queue::Graphics }; }

private:
	struct RayData
	{
		UINT packedData;
		float depth;
		float tCurrent;
		Vector3 rayDirection;
	};

	struct RayResult
	{
		Vector4 radianceOcclusion;
	};

	DeferredVrtResetQueueCS resetQueueCS;
	DeferredVrtGenerateRaysCS generateRaysCS;
	DeferredVrtCoarseTraceRayCS coarseTraceRayCS;
	DeferredVrtTraceRayCS traceRayCS;
	DeferredVrtCollectRaysCS collectRaysCS;

	ComPtr<ID3D12Resource> rayQueues[3];
	ComPtr<ID3D12Resource> queueState;
	ComPtr<ID3D12Resource> dispatchArgs[3];
	ComPtr<ID3D12Resource> rayResults;
	ComPtr<ID3D12CommandSignature> dispatchCommandSignature;

	CbufferView sceneVoxelInfo;
	TextureStatePair normal;
	TextureStatePair depth;
	TextureStatePair output;
	UINT maxRays = 0;
	bool buffersNeedInitialTransition = false;

	void initializeResources(CompositorPass& pass);
	void createDispatchCommandSignature();
	void transitionBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	void uavBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource);
};