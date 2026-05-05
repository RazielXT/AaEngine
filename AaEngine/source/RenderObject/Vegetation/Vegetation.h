#pragma once

#include "RenderCore/RenderSystem.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Compute/ComputeShader.h"
#include "Scene/EntityGeometry.h"
#include "RenderObject/Terrain/TerrainGridParams.h"

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

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* counter);
};

class VegetationUpdateComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* tranformBuffer, ID3D12Resource* counter, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter);
};

struct VegetationChunk
{
	ComPtr<ID3D12Resource> infoBuffer;
	ComPtr<ID3D12Resource> transformationBuffer;
	ComPtr<ID3D12Resource> infoCounter;
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

	void clear();

	void createChunks(RenderWorld& renderWorld, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	void update(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPos, const ProgressiveTerrain& terrain);

	constexpr static UINT ChunksPerTerrainTile = 2;
	constexpr static UINT VegGridSize = ChunksPerTerrainTile * 2;

private:

	VegetationChunk chunks[VegGridSize][VegGridSize];
	XMINT2 gridCenterChunk = { 0, 0 };

	VegetationFindComputeShader vegetationFindCS;
	VegetationUpdateComputeShader vegetationUpdateCS;

	void initializeImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	void createBillboardIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	void initChunk(VegetationChunk& chunk, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, MaterialInstance* material);
	void regenerateChunk(ID3D12GraphicsCommandList* commandList, VegetationChunk& chunk, const ProgressiveTerrain& terrain);
	void updateChunk(ID3D12GraphicsCommandList* commandList, VegetationChunk& chunk);

	ComPtr<ID3D12CommandSignature> commandSignature;
	ComPtr<ID3D12Resource> defaultCommandBuffer;
	ComPtr<ID3D12Resource> zeroCounterBuffer;
	ComPtr<ID3D12Resource> indexBuffer;

	MaterialInstance* vegMaterial{};
};
