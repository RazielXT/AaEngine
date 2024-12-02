#pragma once

#include "ShaderConstantBuffers.h"
#include "GrassInitComputeShader.h"
#include "RenderQueue.h"
#include "AaMath.h"
#include <vector>

class AaEntity;

struct GrassAreaPlacementTask
{
	AaEntity* terrain{};
	BoundingBox bbox;
};

struct GrassAreaDescription
{
	void initialize(BoundingBoxVolume extends);

	DirectX::BoundingBox bbox;

	UINT count{};
	XMUINT2 areaCount{};
	UINT vertexCount{};

	float width = 2;
	float spacing = 2;
};
struct GrassArea
{
	UINT vertexCount{};
	ComPtr<ID3D12Resource> gpuBuffer;
	ComPtr<ID3D12Resource> vertexCounter;
	ComPtr<ID3D12Resource> vertexCounterRead;
};

class GrassAreaGenerator
{
public:

	GrassAreaGenerator();
	~GrassAreaGenerator();

	void initializeGpuResources(RenderSystem* renderSystem, const std::vector<DXGI_FORMAT>& formats);
	void clear();

	void scheduleGrassCreation(GrassAreaPlacementTask grassTask, ID3D12GraphicsCommandList* commandList, const FrameParameters& frame, SceneManager* sceneMgr);
	std::vector<std::pair<AaEntity*, GrassArea*>> finishGrassCreation();

private:


	AaEntity* createGrassEntity(const std::string& name, const GrassAreaDescription& desc, SceneManager* sceneMgr);
	GrassArea* createGrassArea(const GrassAreaDescription& desc);

	GrassInitComputeShader grassCS;

	RenderTargetHeap heap;
	RenderTargetTextures rtt;

	std::vector<std::pair<AaEntity*, GrassArea*>> scheduled;

	std::vector<GrassArea*> grasses;
};