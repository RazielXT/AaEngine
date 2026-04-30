#pragma once

#include "RenderObject/Terrain/GridMesh.h"
#include "RenderObject/Terrain/TerrainGridParams.h"
#include "ResourceUploadBatch.h"
#include "RenderCore/GpuTexture.h"
#include "Resources/Compute/TerrainGenerationCS.h"
#include "Resources/Compute/TextureToMeshCS.h"
#include "Resources/Compute/GenerateMipsComputeShader.h"
#include <functional>

class SceneManager;
class SceneEntity;
class RenderSystem;
struct GraphicsResources;

class ProgressiveTerrain
{
public:

	ProgressiveTerrain();

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& sceneMgr);

	void update(ID3D12GraphicsCommandList* commandList, const Vector3& position, UINT frameIdx);

	bool updateLod = true;

	UINT getCenterHeightmapSrvIndex() const;
	UINT getHeightmapSrvIndex(XMINT2 worldChunk) const;

	GpuTexture2D& getHeightmap(XMINT2 worldChunk);
	Vector3 getHeightmapPosition(XMINT2 worldChunk) const;

	TerrainGridParams params;

	std::vector<XMINT2> regeneratedChunks;

	// Called at the end of update() with the command list, after terrain regeneration.
	// External systems can use this to issue GPU commands (e.g. readback copies).
	std::function<void(ID3D12GraphicsCommandList*, ProgressiveTerrain&)> postUpdateCallback;

	constexpr static UINT GridsSize = TerrainGridParams::GridsSize;

protected:

	GridLODSystem terrainGridTiles;
	GridInstanceMesh terrainGridMesh[GridsSize][GridsSize];

	GpuTexture2D terrainGridHeight[GridsSize][GridsSize];
	GpuTexture2D terrainGridNormal[GridsSize][GridsSize];
	std::vector<ShaderTextureViewUAV> terrainGridNormalMips[GridsSize][GridsSize];

	XMINT2 gridCenterChunk = { 0, 0 };
	XMINT2 chunkWorldCoord[GridsSize][GridsSize]{};
	bool chunkDirty[GridsSize][GridsSize]{};

	VertexBufferModel terrainModel;

	FileTexture* terrainTexture;

	TerrainHeightmapCS generateHeightmapCS;
	GenerateHeightmapNormalsCS heightmapToNormalCS;
	GenerateXYMips4xCS generateNormalMipsCS;

	void regenerateChunk(ID3D12GraphicsCommandList* commandList, int x, int y);
};