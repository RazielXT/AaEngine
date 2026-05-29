#pragma once

#include "Utils/Directx.h"

#include <d3d12.h>
#include <deque>
#include <wrl/client.h>

class VertexBufferModelGarbageCollector
{
public:
	static VertexBufferModelGarbageCollector& Get();

	void enqueue(ID3D12Resource* resource);
	void advanceFrame();
	void flush();

private:
	struct Entry
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint64_t releaseOnFrame = 0;
	};

	std::deque<Entry> resources;
	uint64_t frameCounter = 0;
	static constexpr uint64_t ReleaseDelayFrames = FrameCount;
};
