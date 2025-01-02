#pragma once

#include "ShaderDataBuffers.h"
#include "GrassInitComputeShader.h"
#include "RenderQueue.h"
#include "MathUtils.h"
#include <vector>

class SceneEntity;
class SceneManager;

struct GrassAreaPlacementTask
{
	SceneEntity* terrain{};
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

	void initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, const std::vector<DXGI_FORMAT>& formats);
	void clear();

	void scheduleGrassCreation(GrassAreaPlacementTask grassTask, ID3D12GraphicsCommandList* commandList, const FrameParameters& frame, GraphicsResources& resources, SceneManager& sceneMgr);
	std::vector<std::pair<SceneEntity*, GrassArea*>> finishGrassCreation();

private:

	SceneEntity* createGrassEntity(const std::string& name, const GrassAreaDescription& desc, GraphicsResources& resources, SceneManager& sceneMgr);
	GrassArea* createGrassArea(const GrassAreaDescription& desc, GraphicsResources& resources);

	GrassInitComputeShader grassCS;

	RenderTargetHeap heap;
	RenderTargetTextures rtt;

	std::vector<std::pair<SceneEntity*, GrassArea*>> scheduled;

	std::vector<GrassArea*> grasses;
};