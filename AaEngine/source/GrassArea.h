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

	float width = 0.5f;
	float spacing = 0.5f;
};
struct GrassArea
{
	UINT vertexCount{};
	UINT indexCount{};
	UINT instanceCount{};
	ComPtr<ID3D12Resource> gpuBuffer;
	ComPtr<ID3D12Resource> vertexCounter;
	ComPtr<ID3D12Resource> vertexCounterRead;
	ID3D12Resource* indexBuffer{};
};

class GrassAreaGenerator
{
public:

	GrassAreaGenerator();
	~GrassAreaGenerator();

	void initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);
	void clear();

	void scheduleGrassCreation(GrassAreaPlacementTask grassTask, ID3D12GraphicsCommandList* commandList, const FrameParameters& frame, GraphicsResources& resources, SceneManager& sceneMgr);
	std::vector<std::pair<SceneEntity*, GrassArea*>> finishGrassCreation();

private:

	SceneEntity* createGrassEntity(const std::string& name, const GrassAreaDescription& desc, SceneManager& sceneMgr);
	GrassArea* createGrassArea(const GrassAreaDescription& desc, GraphicsResources& resources);

	GrassInitComputeShader grassCS;

	RenderTargetHeap heap;
	RenderTargetTextures rtt;
	const std::vector<DXGI_FORMAT> formats = { DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R32G32_FLOAT };

	std::vector<std::pair<SceneEntity*, GrassArea*>> scheduled;

	std::vector<GrassArea*> grasses;

	MaterialInstance* defaultMaterial{};

	ComPtr<ID3D12Resource> indexBuffer;
	void createIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
};