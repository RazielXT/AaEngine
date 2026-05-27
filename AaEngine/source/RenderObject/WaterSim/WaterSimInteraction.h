#pragma once

#include "Resources/Compute/WaterInteractionCS.h"
#include <mutex>
#include <vector>

class RenderSystem;
class GpuTexture2D;

class WaterSimInteraction
{
public:
	struct Query
	{
		Vector3 worldPosition;
	};

	struct Result
	{
		float waterHeightDiff;
		Vector2 waterVelocity;
	};

	static constexpr UINT MaxQueries = 512;

	void initializeGpuResources(RenderSystem& renderSystem, const LoadedShader& shader);
	void submitQueries(const std::vector<Query>& queries);
	bool consumeResults(std::vector<Result>& outResults);
	void buildRequest();
	void dispatchCompute(ID3D12GraphicsCommandList* computeList, const GpuTexture2D& waterHeightTexture, const GpuTexture2D& waterVelocityTexture, Vector3 worldCenter, Vector2 worldSize);

private:
	struct Frame
	{
		UINT queryCount = 0;
		bool dispatched = false;
	};

	void readResults(UINT frameIdx);

	WaterInteractionCS computeShader;
	ComPtr<ID3D12Resource> inputBuffer[FrameCount];
	ComPtr<ID3D12Resource> outputBuffer[FrameCount];
	ComPtr<ID3D12Resource> readbackBuffer[FrameCount];
	Frame frames[FrameCount];

	std::mutex mutex;
	std::vector<Query> pendingQueries;
	std::vector<Result> readyResults;
	bool resultsReady = false;
	UINT writeIdx = 0;
};