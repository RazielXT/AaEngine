#include "RenderObject/Terrain/ProgressiveTerrain.h"
#include "Resources/Textures/TextureResources.h"
#include "Resources/GraphicsResources.h"
#include "Scene/RenderWorld.h"
#include <format>
#include "RenderCore/VCT/anisoSeparate/AnisoSeparateVoxelization.h"
#include "Utils/Logger.h"

const UINT TextureSize = 1024;
const UINT TerrainModelSize = 33;

ProgressiveTerrain::ProgressiveTerrain()
{
}

ProgressiveTerrain::~ProgressiveTerrain()
{
	updateCtx.running = false;

	for (auto& work : chunkUpdateThreads)
	{
		SetEvent(work.eventBegin);
		work.worker.join();
		CloseHandle(work.eventBegin);
		CloseHandle(work.eventFinish);
	}
}

void ProgressiveTerrain::initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, RenderWorld& renderWorld)
{
	terrainTexture = resources.textures.loadFile(*renderSystem.core.device, batch, "Mountain Range Height Map PNG.png");
	resources.descriptors.createTextureView(*terrainTexture);

	params.tileSize = 1000.f;
	params.tileHeight = 1000.f;
	params.tileCenterOffset = { params.tileSize * -0.5f, params.tileHeight * -0.5f, params.tileSize * -0.5f };

	terrainGridTiles.Initialize({ params.tileSize, params.tileSize }, params.tileCenterOffset, 9);

	for (auto& s : shadowTerrainGridTiles)
		s.Initialize({ params.tileSize, params.tileSize }, params.tileCenterOffset, 9);

	terrainVoxelizeGridTiles[0].Initialize({ params.tileSize, params.tileSize }, params.tileCenterOffset, 7);
	terrainVoxelizeGridTiles[1].Initialize({ params.tileSize, params.tileSize }, params.tileCenterOffset, 7);
	terrainVoxelizeGridTiles[2].Initialize({ params.tileSize, params.tileSize }, params.tileCenterOffset, 5);
	terrainVoxelizeGridTiles[3].Initialize({ params.tileSize, params.tileSize }, params.tileCenterOffset, 5);

	terrainModel.CreateIndexBufferGrid(renderSystem.core.device, &batch, TerrainModelSize);
	terrainModel.bbox.Extents = { params.tileSize * 0.5f, params.tileHeight * 0.5f, params.tileSize * 0.5f };
	terrainModel.bbox.Center = { params.tileSize * 0.5f, params.tileHeight * 0.5f, params.tileSize * 0.5f };

	for (int x = 0; x < GridsSize; x++)
		for (int y = 0; y < GridsSize; y++)
		{
			auto& terrainHeight = terrainGridHeight[x][y];
			auto& terrainNormal = terrainGridNormal[x][y];
			auto& terrainNormalMips = terrainGridNormalMips[x][y];

			terrainHeight.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R16_UNORM, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			terrainHeight.SetName("TerrainHeight");
			resources.descriptors.createUAVView(terrainHeight);
			resources.descriptors.createTextureView(terrainHeight);

			terrainNormal.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R8G8_SNORM, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, { .mipLevels = 5 });
			terrainNormal.SetName("TerrainNormal");
			resources.descriptors.createUAVView(terrainNormal);
			resources.descriptors.createTextureView(terrainNormal);
			terrainNormalMips = resources.descriptors.createUAVMips(terrainNormal);

			auto& chunk = terrainGrid[x][y];
			chunk.mesh.create(terrainGridTiles.MaxInstances);

			for (auto& m : chunk.shadowMesh)
				m.create(terrainGridTiles.MaxInstances);

			for (auto& vm : chunk.voxelizeMesh)
				vm.create(terrainVoxelizeGridTiles[0].MaxInstances);

			auto e = chunk.entity = renderWorld.createEntity();
			e->geometry.fromInstancedModel(terrainModel, 0, chunk.mesh.gpuBuffer.data[0].GpuAddress());
			e->setBoundingBox(terrainModel.bbox);
			e->material = resources.materials.getMaterial("MountainTerrainGrid", batch);
			e->Material().setParam("TexIdHeightmap", terrainHeight.view.srvHeapIndex);
			e->Material().setParam("TexIdNormalmap", terrainNormal.view.srvHeapIndex);
			e->Material().setParam("GridHeightWidth", Vector2(params.tileHeight, params.tileSize));
			e->Material().setParam("GridIndex", x + y * GridsSize);
			e->Material().setParam("GridTilesWidth", terrainGridTiles.TilesWidth);
			e->Material().setParam("GridTilesWidth", terrainVoxelizeGridTiles[0].TilesWidth, RenderViewId_Voxelize0);
			e->Material().setParam("GridTilesWidth", terrainVoxelizeGridTiles[1].TilesWidth, RenderViewId_Voxelize1);
			e->Material().setParam("GridTilesWidth", terrainVoxelizeGridTiles[2].TilesWidth, RenderViewId_Voxelize2);
			e->Material().setParam("GridTilesWidth", terrainVoxelizeGridTiles[3].TilesWidth, RenderViewId_Voxelize3);
			e->Material().setParam("GridTilesWidth", terrainVoxelizeGridTiles[3].TilesWidth, RenderViewId_VoxelizeShadowMap);

			e->geometry.viewVariants = &chunk.geometryViews.variants;

			for (auto& g : chunk.geometryViews.viewGeometries)
				g = e->geometry;

			// Toroidal-consistent world coord: slot x maps to world x for x<=2, x-5 for x>2
			// This ensures wrapIndex(worldCoord, GridsSize) == slot index
			int wx = x > (int)GridsSize / 2 ? x - (int)GridsSize : x;
			int wy = y > (int)GridsSize / 2 ? y - (int)GridsSize : y;

			chunkWorldCoord[x][y] = { wx, wy };
			e->setPosition(params.chunkWorldMin(chunkWorldCoord[x][y], params.tileSize));
			chunkDirty[x][y] = true;
		}

	for (UINT i = 0; auto& work : chunkUpdateThreads)
	{
		work.eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
		work.eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);

		work.worker = std::thread([this, &work, &renderSystem, idx = i++]
			{
				while (WaitForSingleObject(work.eventBegin, INFINITE) == WAIT_OBJECT_0 && updateCtx.running)
				{
					runChunkUpdateThread(idx);
					SetEvent(work.eventFinish);
				}
			});
	}

	auto csShader = resources.shaders.getShader("generateHeightmapNormalsCS", ShaderType::Compute, ShaderRef{ "grid/generateHeightmapNormalsCS.hlsl", "CSMain", "cs_6_6" });
	heightmapToNormalCS.init(*renderSystem.core.device, *csShader);

	generateNormalMipsCS.init(*renderSystem.core.device, "generateXYMips4xCS", resources.shaders);

	csShader = resources.shaders.getShader("generateHeightmapCS", ShaderType::Compute, ShaderRef{ "terrain/terrainGridHeightmapCS.hlsl", "CSMain", "cs_6_6" });
	generateHeightmapCS.init(*renderSystem.core.device, *csShader);
}

void ProgressiveTerrain::regenerateChunk(ID3D12GraphicsCommandList* commandList, int x, int y)
{
	auto& terrainHeight = terrainGridHeight[x][y];
	auto& terrainNormal = terrainGridNormal[x][y];
	auto& terrainNormalMips = terrainGridNormalMips[x][y];

	TextureTransitions<3> tr;
	tr.add(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.add(&terrainNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tr.add(&terrainHeight, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tr.push(commandList);

	generateHeightmapCS.dispatch(commandList, TextureSize, TextureSize, terrainHeight.view.uavHandle, { chunkWorldCoord[x][y], terrainTexture->srvHeapIndex });

	auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(terrainHeight.texture.Get());
	commandList->ResourceBarrier(1, &uavb);

	heightmapToNormalCS.dispatch(commandList, terrainHeight.view.srvHeapIndex, terrainNormal.view.uavHeapIndex, TextureSize, TextureSize, 50, 102.4f);

	auto uavb2 = CD3DX12_RESOURCE_BARRIER::UAV(terrainNormal.texture.Get());
	commandList->ResourceBarrier(1, &uavb2);

	generateNormalMipsCS.dispatch(commandList, TextureSize, terrainNormalMips);

	tr.add(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.add(&terrainNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.add(&terrainHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.push(commandList);
}

void ProgressiveTerrain::update(ID3D12GraphicsCommandList* commandList, const Camera& camera, const ShadowMaps& shadows, UINT frameIdx)
{
	XMINT2 cameraChunk = params.gridCenterAt(camera.getPosition(), params.tileSize, GridsSize);

	if (cameraChunk.x != gridCenterChunk.x || cameraChunk.y != gridCenterChunk.y)
	{
		gridCenterChunk = cameraChunk;

		for (int x = 0; x < (int)GridsSize; x++)
			for (int y = 0; y < (int)GridsSize; y++)
			{
				XMINT2 desired = { gridCenterChunk.x + x - 2, gridCenterChunk.y + y - 2 };
				int ax = TerrainGridParams::wrapIndex(desired.x, GridsSize);
				int ay = TerrainGridParams::wrapIndex(desired.y, GridsSize);

				if (chunkWorldCoord[ax][ay].x != desired.x || chunkWorldCoord[ax][ay].y != desired.y)
				{
					chunkWorldCoord[ax][ay] = desired;
					chunkDirty[ax][ay] = true;

					terrainGrid[ax][ay].entity->setPosition(params.chunkWorldMin(desired, params.tileSize));
				}
			}
	}

	regeneratedChunks.clear();

	for (int x = 0; x < (int)GridsSize; x++)
		for (int y = 0; y < (int)GridsSize; y++)
		{
			if (chunkDirty[x][y])
			{
				regenerateChunk(commandList, x, y);
				chunkDirty[x][y] = false;
				regeneratedChunks.push_back(chunkWorldCoord[x][y]);
			}
		}

	dispatchChunkUpdates(camera, shadows, frameIdx);

	if (postUpdateCallback)
		postUpdateCallback(commandList, *this);
}

void ProgressiveTerrain::updateFinish()
{
	waitForChunkUpdates();
}

void ProgressiveTerrain::dispatchChunkUpdates(const Camera& camera, const ShadowMaps& shadows, UINT frameIdx)
{
	updateCtx.camera = &camera;
	updateCtx.shadows = &shadows;
	updateCtx.frameIdx = frameIdx;

	for (auto& work : chunkUpdateThreads)
	{
		SetEvent(work.eventBegin);
	}
}

void ProgressiveTerrain::runChunkUpdateThread(UINT threadIndex)
{
	if (!updateLod)
		return;

	const auto& camera = *updateCtx.camera;

	BoundingOrientedBox shadowBbox;
	if (threadIndex >= 1 && threadIndex <= 4) //shadow camera 1-4
	{
		UINT i = threadIndex - 1;
		auto& shadowCamera = updateCtx.shadows->cascades[i].camera;
		shadowBbox = shadowCamera.prepareOrientedBox();
	}

	for (int x = 0; x < (int)GridsSize; x++)
		for (int y = 0; y < (int)GridsSize; y++)
		{
			auto& grid = terrainGrid[x][y];

			if (threadIndex == 0) // main camera
			{
				terrainGridTiles.BuildLOD(camera.getPosition(), camera.prepareFrustum(), chunkWorldCoord[x][y]);
				grid.mesh.update(grid.entity->geometry, (UINT)terrainGridTiles.m_renderList.size(), terrainGridTiles.m_renderList.data(), (UINT)terrainGridTiles.m_renderList.size() * sizeof(TileData), updateCtx.frameIdx);
			}
			else if (threadIndex >= 1 && threadIndex <= 4) //shadow camera 1-4
			{
				UINT i = threadIndex - 1;
				auto& tiles = shadowTerrainGridTiles[i];

				const UINT tileLod[] = { 9, 9, 7, 6 };
				tiles.BuildLOD(camera.getPosition(), shadowBbox, chunkWorldCoord[x][y], tileLod[i]);

				auto& g = grid.geometryViews.viewGeometries[threadIndex - 1];
				grid.shadowMesh[i].update(g, (UINT)tiles.m_renderList.size(), tiles.m_renderList.data(), (UINT)tiles.m_renderList.size() * sizeof(TileData), updateCtx.frameIdx);
			}
			else //voxelize camera 1-4
			{
				UINT i = threadIndex - 5;

				auto cascadeExtends = AnisoSeparateVoxelization::CascadeExtends[i];
				Vector2 voxelizeExtends = Vector2(cascadeExtends, cascadeExtends) * 1.5f;
				Vector2 p = { camera.getPosition().x, camera.getPosition().z };

				auto& tiles = terrainVoxelizeGridTiles[i];
				tiles.BuildAabbLOD(camera.getPosition(), chunkWorldCoord[x][y], p - voxelizeExtends, p + voxelizeExtends);
				//tiles.BuildLOD(camera.getPosition(), chunkWorldCoord[x][y]);

				auto& g = grid.geometryViews.viewGeometries[threadIndex - 1];
				grid.voxelizeMesh[i].update(g, (UINT)tiles.m_renderList.size(), tiles.m_renderList.data(), (UINT)tiles.m_renderList.size() * sizeof(TileData), updateCtx.frameIdx);

				if (i == 3) // shadowmap uses last cascade tiles
				{
					auto& g = grid.geometryViews.viewGeometries[threadIndex];
					grid.voxelizeMesh[3].update(g, (UINT)tiles.m_renderList.size(), tiles.m_renderList.data(), (UINT)tiles.m_renderList.size() * sizeof(TileData), updateCtx.frameIdx);
				}
			}
		}
}

void ProgressiveTerrain::waitForChunkUpdates()
{
	for (auto& work : chunkUpdateThreads)
	{
		WaitForSingleObject(work.eventFinish, INFINITE);
	}
}

UINT ProgressiveTerrain::getCenterHeightmapSrvIndex() const
{
	int ax = TerrainGridParams::wrapIndex(gridCenterChunk.x, GridsSize);
	int ay = TerrainGridParams::wrapIndex(gridCenterChunk.y, GridsSize);
	return terrainGridHeight[ax][ay].view.srvHeapIndex;
}

UINT ProgressiveTerrain::getHeightmapSrvIndex(XMINT2 worldChunk) const
{
	int ax = TerrainGridParams::wrapIndex(worldChunk.x, GridsSize);
	int ay = TerrainGridParams::wrapIndex(worldChunk.y, GridsSize);
	return terrainGridHeight[ax][ay].view.srvHeapIndex;
}

GpuTexture2D& ProgressiveTerrain::getHeightmap(XMINT2 worldChunk)
{
	int ax = TerrainGridParams::wrapIndex(worldChunk.x, GridsSize);
	int ay = TerrainGridParams::wrapIndex(worldChunk.y, GridsSize);
	return terrainGridHeight[ax][ay];
}

DirectX::SimpleMath::Vector3 ProgressiveTerrain::getHeightmapPosition(XMINT2 worldChunk) const
{
	return params.chunkWorldMin(chunkWorldCoord[worldChunk.x][worldChunk.y], params.tileSize);
}
