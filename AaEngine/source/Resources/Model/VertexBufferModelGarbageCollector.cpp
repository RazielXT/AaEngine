#include "Resources/Model/VertexBufferModelGarbageCollector.h"

VertexBufferModelGarbageCollector& VertexBufferModelGarbageCollector::Get()
{
	static VertexBufferModelGarbageCollector collector;
	return collector;
}

void VertexBufferModelGarbageCollector::enqueue(ID3D12Resource* resource)
{
	if (!resource)
		return;

	Entry entry;
	entry.resource.Attach(resource);
	entry.releaseOnFrame = frameCounter + ReleaseDelayFrames;
	resources.push_back(std::move(entry));
}

void VertexBufferModelGarbageCollector::advanceFrame()
{
	++frameCounter;
	while (!resources.empty() && resources.front().releaseOnFrame <= frameCounter)
		resources.pop_front();
}

void VertexBufferModelGarbageCollector::flush()
{
	resources.clear();
}
