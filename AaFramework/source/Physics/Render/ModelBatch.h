#pragma once

#include "../JoltHeader.h"
#include "Resources/Model/VertexBufferModel.h"
#include "Jolt/Core/Reference.h"
#include "Utils/Directx.h"
#include <deque>

class ModelBatch;

class ModelBatchGarbageCollector
{
public:

	ModelBatchGarbageCollector() = default;
	~ModelBatchGarbageCollector();

	void EnqueueBatchForDeletion(ModelBatch* batchToDelete);
	void CollectGarbageBatches();

private:

	struct DeletedBatchEntry
	{
		ModelBatch* batch{};
		uint64_t deleteOnFrame = 0;
	};

	std::deque<DeletedBatchEntry> batchTrash;
	uint64_t frameCounter = 0;
	static constexpr uint64_t BatchDeleteDelayFrames = FrameCount;
};

class ModelBatch : public JPH::RefTargetVirtual, public JPH::RefTarget<ModelBatch>
{
public:
	ModelBatch(ModelBatchGarbageCollector& owner) : owner(owner)
	{
		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
		model.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
		model.addLayoutElement(DXGI_FORMAT_R8G8B8A8_UNORM, VertexElementSemantic::COLOR);
	}

	VertexBufferModel model;

	void AddRef() override { RefTarget::AddRef(); }
	void Release() override
	{
		if (--mRefCount == 0)
		{
			owner.EnqueueBatchForDeletion(this);
		}
	}

private:
	ModelBatchGarbageCollector& owner;
};
