#pragma once

#include "RenderObject/Terrain/GridMesh.h"
#include "ResourceUploadBatch.h"
#include "RenderCore/GpuTexture.h"
#include "Resources/Compute/TerrainGenerationCS.h"
#include "Resources/Compute/TextureToMeshCS.h"
#include "Resources/Compute/GenerateMipsComputeShader.h"

class SceneManager;
class SceneEntity;
class RenderSystem;
struct GraphicsResources;

class ProgressiveTerrain
{
public:

	ProgressiveTerrain();

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& sceneMgr);

	void createTerrain(ID3D12GraphicsCommandList* commandList, RenderSystem& rs, SceneManager& sceneMgr, GraphicsResources& resources, ResourceUploadBatch& batch);
	void update(ID3D12GraphicsCommandList* commandList, const Vector3& position, UINT frameIdx);

	bool updateLod = true;

	UINT getCenterHeightmapSrvIndex() const;

//protected:

	constexpr static UINT GridsSize = 5;

	GridLODSystem terrainGridTiles;
	GridInstanceMesh terrainGridMesh[GridsSize][GridsSize];

	GpuTexture2D terrainGridHeight[GridsSize][GridsSize];
	GpuTexture2D terrainGridNormal[GridsSize][GridsSize];
	std::vector<ShaderTextureViewUAV> terrainGridNormalMips[GridsSize][GridsSize];

	XMINT2 gridCenterChunk = { 0, 0 };
	XMINT2 chunkWorldCoord[GridsSize][GridsSize]{};
	bool chunkDirty[GridsSize][GridsSize]{};

	float gridTileSize = 0;
	float gridTileHeight = 0;
	Vector3 terrainCenterPosition;

	VertexBufferModel terrainModel;

	FileTexture* terrainTexture;

	TerrainHeightmapCS generateHeightmapCS;
	GenerateHeightmapNormalsCS heightmapToNormalCS;
	GenerateNormalMips4xCS generateNormalMipsCS;

	void regenerateChunk(ID3D12GraphicsCommandList* commandList, int x, int y);
};