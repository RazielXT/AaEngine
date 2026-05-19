#include "ModelBatch.h"

ModelBatchGarbageCollector::~ModelBatchGarbageCollector()
{
	for (auto& entry : batchTrash)
		delete entry.batch;
}

void ModelBatchGarbageCollector::EnqueueBatchForDeletion(ModelBatch* batchToDelete)
{
	batchTrash.push_back({ batchToDelete, frameCounter + BatchDeleteDelayFrames });
}

void ModelBatchGarbageCollector::CollectGarbageBatches()
{
	while (!batchTrash.empty() && batchTrash.front().deleteOnFrame <= frameCounter)
	{
		delete batchTrash.front().batch;
		batchTrash.pop_front();
	}
}