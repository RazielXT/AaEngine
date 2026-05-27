#include "RenderObject/WaterSim/WaterSimInteraction.h"
#include "RenderCore/GpuTexture.h"
#include "RenderCore/RenderSystem.h"
#include <algorithm>

void WaterSimInteraction::initializeGpuResources(RenderSystem& renderSystem, const LoadedShader& shader)
{
	computeShader.init(*renderSystem.core.device, shader);

	const UINT inputStride = sizeof(float) * 4;
	const UINT outputStride = sizeof(float) * 4;
	const UINT inputBufferSize = MaxQueries * inputStride;
	const UINT outputBufferSize = MaxQueries * outputStride;

	auto* device = renderSystem.core.device;
	for (UINT i = 0; i < FrameCount; i++)
	{
		D3D12_HEAP_PROPERTIES uploadHeap = {};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC bufDesc = {};
		bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufDesc.Width = inputBufferSize;
		bufDesc.Height = 1;
		bufDesc.DepthOrArraySize = 1;
		bufDesc.MipLevels = 1;
		bufDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufDesc.SampleDesc.Count = 1;
		bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&inputBuffer[i]));
		inputBuffer[i]->SetName(L"WaterInteractionInput");

		D3D12_HEAP_PROPERTIES defaultHeap = {};
		defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		bufDesc.Width = outputBufferSize;
		bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&outputBuffer[i]));
		outputBuffer[i]->SetName(L"WaterInteractionOutput");

		D3D12_HEAP_PROPERTIES readbackHeap = {};
		readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
		bufDesc.Width = outputBufferSize;
		bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer[i]));
		readbackBuffer[i]->SetName(L"WaterInteractionReadback");
	}
}

void WaterSimInteraction::submitQueries(const std::vector<Query>& queries)
{
	std::lock_guard lock(mutex);
	pendingQueries = queries;
}

bool WaterSimInteraction::consumeResults(std::vector<Result>& outResults)
{
	std::lock_guard lock(mutex);
	if (!resultsReady)
		return false;

	outResults = std::move(readyResults);
	resultsReady = false;
	return true;
}

void WaterSimInteraction::buildRequest()
{
	const UINT readIdx = (writeIdx + 1) % FrameCount;
	readResults(readIdx);

	std::vector<Query> queries;
	{
		std::lock_guard lock(mutex);
		queries = std::move(pendingQueries);
	}

	auto& frame = frames[writeIdx];
	frame.queryCount = static_cast<UINT>((std::min)(queries.size(), static_cast<size_t>(MaxQueries)));
	frame.dispatched = frame.queryCount > 0;

	if (frame.queryCount == 0)
		return;

	struct InputEntry { float x, y, z, pad; };
	InputEntry* mapped = nullptr;
	inputBuffer[writeIdx]->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	for (UINT i = 0; i < frame.queryCount; i++)
	{
		mapped[i] = { queries[i].worldPosition.x, queries[i].worldPosition.y, queries[i].worldPosition.z, 0 };
	}
	D3D12_RANGE written = { 0, frame.queryCount * sizeof(InputEntry) };
	inputBuffer[writeIdx]->Unmap(0, &written);
}

void WaterSimInteraction::dispatchCompute(ID3D12GraphicsCommandList* computeList, const GpuTexture2D& waterHeightTexture, const GpuTexture2D& waterVelocityTexture, Vector3 worldCenter, Vector2 worldSize)
{
	auto& frame = frames[writeIdx];
	if (!frame.dispatched)
	{
		writeIdx = (writeIdx + 1) % FrameCount;
		return;
	}

	CommandsMarker marker(computeList, "WaterInteraction", PixColor::ForestGreen);

	WaterInteractionCS::DispatchParams params;
	params.queryCount = frame.queryCount;
	params.waterHeightSrvIndex = waterHeightTexture.view.srvHeapIndex;
	params.waterVelocitySrvIndex = waterVelocityTexture.view.srvHeapIndex;
	params.worldCenter = { worldCenter.x, worldCenter.z };
	params.worldSize = worldSize;
	params.waterHeight = 1000;
	params.waterHeightStart = -500;

	computeShader.dispatch(computeList, params, inputBuffer[writeIdx].Get(), outputBuffer[writeIdx].Get());

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(outputBuffer[writeIdx].Get());
	computeList->ResourceBarrier(1, &uavBarrier);

	auto transitionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		outputBuffer[writeIdx].Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	computeList->ResourceBarrier(1, &transitionBarrier);

	const UINT copySize = frame.queryCount * sizeof(float) * 4;
	computeList->CopyBufferRegion(readbackBuffer[writeIdx].Get(), 0, outputBuffer[writeIdx].Get(), 0, copySize);

	transitionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		outputBuffer[writeIdx].Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeList->ResourceBarrier(1, &transitionBarrier);

	writeIdx = (writeIdx + 1) % FrameCount;
}

void WaterSimInteraction::readResults(UINT readIdx)
{
	auto& frame = frames[readIdx];
	if (!frame.dispatched || frame.queryCount == 0)
		return;

	struct OutputEntry { float heightDiff; float velX, velY; float pad; };

	D3D12_RANGE readRange = { 0, frame.queryCount * sizeof(OutputEntry) };
	OutputEntry* mapped = nullptr;
	readbackBuffer[readIdx]->Map(0, &readRange, reinterpret_cast<void**>(&mapped));

	std::vector<Result> results(frame.queryCount);
	for (UINT i = 0; i < frame.queryCount; i++)
	{
		results[i].waterHeightDiff = mapped[i].heightDiff;
		results[i].waterVelocity = { mapped[i].velX, mapped[i].velY };
	}

	D3D12_RANGE emptyRange = { 0, 0 };
	readbackBuffer[readIdx]->Unmap(0, &emptyRange);

	{
		std::lock_guard lock(mutex);
		readyResults = std::move(results);
		resultsReady = true;
	}

	frame.dispatched = false;
	frame.queryCount = 0;
}