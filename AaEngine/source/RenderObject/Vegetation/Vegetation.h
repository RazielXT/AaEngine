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
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* subgroupMeta);
};

class VegetationUpdateComputeShader : public ComputeShader
{
public:

	struct Input
	{
		XMFLOAT4 frustumPlanes[6];
		XMFLOAT3 chunkWorldMin;
		float chunkSize;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* subgroupMeta, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr, ID3D12Resource* redirectBuffer);
};

struct VegetationChunk
{
	ComPtr<ID3D12Resource> infoBuffer;
	ComPtr<ID3D12Resource> subgroupMetaBuffer;
	ComPtr<ID3D12Resource> redirectBuffer;
	IndirectEntityGeometry impostors;
	RenderEntity* entity{};

	XMINT2 worldCoord{};
	bool dirty = true;
	bool firstRun = true;
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
	void updateCulling(ID3D12GraphicsCommandList* commandList, const Camera& camera, const ProgressiveTerrain& terrain);

	constexpr static UINT ChunksPerTerrainTile = 2;
	constexpr static UINT VegGridSize = ChunksPerTerrainTile * 2;

	constexpr static UINT SubgroupsPerDim = 8;
	constexpr static UINT SubgroupCount = SubgroupsPerDim * SubgroupsPerDim;
	constexpr static UINT TotalChunks = VegGridSize * VegGridSize;
	constexpr static UINT MaxItemsPerSubgroup = 1024;

private:

	VegetationChunk chunks[VegGridSize][VegGridSize];
	XMINT2 gridCenterChunk = { 0, 0 };

	VegetationFindComputeShader vegetationFindCS;
	IndirectDrawIndexedClearCS indirectDrawClearCS;
	VegetationUpdateComputeShader vegetationUpdateCS;
	bool updatingEnabled = true;

	void initializeImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	void createBillboardIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	void initChunk(VegetationChunk& chunk, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, MaterialInstance* material);
	void regenerateChunk(ID3D12GraphicsCommandList* commandList, VegetationChunk& chunk, const ProgressiveTerrain& terrain);
	void updateChunk(ID3D12GraphicsCommandList* commandList, VegetationChunk& chunk, const VegetationUpdateComputeShader::Input& cullingInput);

	ComPtr<ID3D12CommandSignature> commandSignature;
	ComPtr<ID3D12Resource> sharedCommandBuffer;
	ComPtr<ID3D12Resource> defaultMetaBuffer;
	ComPtr<ID3D12Resource> indexBuffer;

	MaterialInstance* vegMaterial{};
};
