#pragma once

#include "RenderObject/Terrain/GridMesh.h"
#include "RenderObject/Terrain/TerrainGridParams.h"
#include "ResourceUploadBatch.h"
#include "RenderCore/GpuTexture.h"
#include "Resources/Compute/TerrainGenerationCS.h"
#include "Resources/Compute/TextureToMeshCS.h"
#include "Resources/Compute/GenerateMipsComputeShader.h"
#include <functional>

class RenderWorld;
class RenderEntity;
class RenderSystem;
struct GraphicsResources;
class ShadowMaps;

class ProgressiveTerrain
{
public:

	ProgressiveTerrain();
	~ProgressiveTerrain();

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, RenderWorld& renderWorld);

	void update(ID3D12GraphicsCommandList* commandList, const Camera& cam, const ShadowMaps& shadows, UINT frameIdx);
	void updateFinish();

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
	GridLODSystem shadowTerrainGridTiles[4];
	GridLODSystem terrainVoxelizeGridTiles[4];

	struct TerrainGridChunk
	{
		GridInstanceMesh mesh;
		GridInstanceMesh shadowMesh[4];
		GridInstanceMesh voxelizeMesh[4];

		GeometryViews<RenderViewId_ShadowCascade0, RenderViewId_ShadowCascade1, RenderViewId_ShadowCascade2, RenderViewId_ShadowCascade3, RenderViewId_Voxelize0, RenderViewId_Voxelize1, RenderViewId_Voxelize2, RenderViewId_Voxelize3, RenderViewId_VoxelizeShadowMap> geometryViews;
		RenderEntity* entity{};
	}
	terrainGrid[GridsSize][GridsSize];

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

	void dispatchChunkUpdates(const Camera& camera, const ShadowMaps& shadows, UINT frameIdx);
	void runChunkUpdateThread(UINT threadIndex);
	void waitForChunkUpdates();

	struct AsyncWork
	{
		HANDLE eventBegin{};
		HANDLE eventFinish{};
		std::thread worker;
	};
	std::array<AsyncWork, 1 + 4 + 4> chunkUpdateThreads;

	struct 
	{
		const Camera* camera{};
		const ShadowMaps* shadows{};
		UINT frameIdx{};
		bool running = true;
	}
	updateCtx;
};