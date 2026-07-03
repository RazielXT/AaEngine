#pragma once

#include "RenderCore/RenderSystem.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Compute/ComputeShader.h"
#include "Scene/EntityGeometry.h"
#include "RenderObject/Terrain/TerrainGridParams.h"
#include "Resources/Compute/IndirectCommandsCS.h"

class RenderWorld;
class ProgressiveTerrain;

class VegetationFindComputeShader : public ComputeShader
{
public:

	struct Input
	{
		UINT terrainTexture;
		float terrainHeight;
		Vector2 terrainOffset;
		Vector2 chunkWorldSize;
		Vector2 subUvOffset;
		Vector2 subUvScale;
		UINT noiseTexture;
		UINT subgroupBase;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* subgroupMeta);
};

class VegetationUpdateComputeShader : public ComputeShader
{
public:

	struct Input
	{
		XMFLOAT4 frustumPlanes[6];
		float chunkSize;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* subgroupMeta, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr, ID3D12Resource* redirectBuffer, UINT subgroupCount);
};

struct VegetationTile
{
	XMINT2 worldCoord{};
	bool dirty = true;
};

class Vegetation
{
public:

	Vegetation();

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);
	void enableUpdating(bool enabled);

	void clear();

	void createChunks(RenderWorld& renderWorld, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	void update(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPos, const ProgressiveTerrain& terrain);
	void updateCulling(ID3D12GraphicsCommandList* commandList, const Camera& camera, const ProgressiveTerrain& terrain, UINT viewIndex = 0);

	constexpr static UINT ChunksPerTerrainTile = 2;
	constexpr static UINT VegGridSize = ChunksPerTerrainTile * 2;

	constexpr static UINT SubgroupsPerDim = 8;
	constexpr static UINT SubgroupCount = SubgroupsPerDim * SubgroupsPerDim;
	constexpr static UINT TotalChunks = VegGridSize * VegGridSize;
	constexpr static UINT MaxItemsPerSubgroup = 1024;

	// All chunks share one info/metadata buffer; each chunk owns a contiguous block of SubgroupCount subgroups.
	constexpr static UINT TotalSubgroups = TotalChunks * SubgroupCount;
	constexpr static UINT MaxTotalItems = TotalSubgroups * MaxItemsPerSubgroup;

	// Only the main view is culled today, but the buffers are per-view to allow shadow views later (like grass).
	constexpr static UINT ViewCount = 1;

private:

	VegetationTile tiles[VegGridSize][VegGridSize];
	XMINT2 gridCenterChunk = { 0, 0 };

	VegetationFindComputeShader vegetationFindCS;
	IndirectDrawIndexedClearCS indirectDrawClearCS;
	VegetationUpdateComputeShader vegetationUpdateCS;
	bool updatingEnabled = true;

	void initializeImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	void createBillboardIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	void createSharedBuffers(GraphicsResources& resources);
	void regenerateDirtyTiles(ID3D12GraphicsCommandList* commandList, const ProgressiveTerrain& terrain);

	ComPtr<ID3D12CommandSignature> commandSignature;
	ComPtr<ID3D12Resource> sharedCommandBuffer;
	ComPtr<ID3D12Resource> defaultMetaBuffer;
	ComPtr<ID3D12Resource> indexBuffer;

	ComPtr<ID3D12Resource> infoBuffer;
	ComPtr<ID3D12Resource> subgroupMetaBuffer;
	ComPtr<ID3D12Resource> redirectBuffer[ViewCount];
	IndirectEntityGeometry indirect[ViewCount];
	RenderEntity* entity{};

	MaterialInstance* vegMaterial{};
	FileTexture* noiseTexture{};
};
