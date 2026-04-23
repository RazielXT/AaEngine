#include "RenderObject/Terrain/ProgressiveTerrain.h"
#include "Resources/Textures/TextureResources.h"
#include "Resources/GraphicsResources.h"
#include "Scene/SceneManager.h"
#include <format>

const UINT TextureSize = 1024;
const UINT TerrainModelSize = 33;

ProgressiveTerrain::ProgressiveTerrain()
{
}

void ProgressiveTerrain::initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& sceneMgr)
{
	terrainTexture = resources.textures.loadFile(*renderSystem.core.device, batch, "Mountain Range Height Map PNG.png");
	resources.descriptors.createTextureView(*terrainTexture);

	params.tileSize = 8000.f;
	params.tileHeight = 8000.f;
	params.tileCenterOffset = { params.tileSize * -0.5f, params.tileHeight * -0.5f, params.tileSize * -0.5f };

	terrainGridTiles.Initialize({ params.tileSize, params.tileSize }, params.tileCenterOffset, 10);

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

			auto e = sceneMgr.createEntity("Terrain" + std::format("{}{}", x, y));
			terrainGridMesh[x][y].create(terrainGridTiles.TilesWidth);
			terrainGridMesh[x][y].entity = e;
			e->geometry.fromInstancedModel(terrainModel, 0, terrainGridMesh[x][y].gpuBuffer.data[0].GpuAddress());
			e->setBoundingBox(terrainModel.bbox);
			e->material = resources.materials.getMaterial("MountainTerrainGrid", batch);
			e->Material().setParam("TexIdHeightmap", terrainHeight.view.srvHeapIndex);
			e->Material().setParam("TexIdNormalmap", terrainNormal.view.srvHeapIndex);
			e->Material().setParam("GridHeightWidth", Vector2(params.tileHeight, params.tileSize));
			e->Material().setParam("GridIndex", x + y * GridsSize);

			// Toroidal-consistent world coord: slot x maps to world x for x<=2, x-5 for x>2
			// This ensures wrapIndex(worldCoord, GridsSize) == slot index
			int wx = x > (int)GridsSize / 2 ? x - (int)GridsSize : x;
			int wy = y > (int)GridsSize / 2 ? y - (int)GridsSize : y;

			chunkWorldCoord[x][y] = { wx, wy };
			e->setPosition(params.chunkWorldMin(chunkWorldCoord[x][y], params.tileSize));
			chunkDirty[x][y] = true;
		}

	auto csShader = resources.shaders.getShader("generateHeightmapNormalsCS", ShaderType::Compute, ShaderRef{ "grid/generateHeightmapNormalsCS.hlsl", "CSMain", "cs_6_6" });
	heightmapToNormalCS.init(*renderSystem.core.device, *csShader);

	generateNormalMipsCS.init(*renderSystem.core.device, "generateNormalMipmaps4x", resources.shaders);

	csShader = resources.shaders.getShader("generateHeightmapCS", ShaderType::Compute, ShaderRef{ "terrain/terrainGridHeightmapDefinedCS.hlsl", "CSMain", "cs_6_6" });
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

void ProgressiveTerrain::update(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPos, UINT frameIdx)
{
	XMINT2 cameraChunk = params.gridCenterAt(cameraPos, params.tileSize, GridsSize);

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

				terrainGridMesh[ax][ay].entity->setPosition(params.chunkWorldMin(desired, params.tileSize));
				}
			}
	}

	for (int x = 0; x < (int)GridsSize; x++)
		for (int y = 0; y < (int)GridsSize; y++)
		{
			if (chunkDirty[x][y])
			{
				regenerateChunk(commandList, x, y);
				chunkDirty[x][y] = false;
			}
		}

	if (updateLod)
	{
		for (int x = 0; x < (int)GridsSize; x++)
			for (int y = 0; y < (int)GridsSize; y++)
			{
				terrainGridTiles.BuildLOD(cameraPos, chunkWorldCoord[x][y]);
				terrainGridMesh[x][y].update((UINT)terrainGridTiles.m_renderList.size(), terrainGridTiles.m_renderList.data(), (UINT)terrainGridTiles.m_renderList.size() * sizeof(TileData), frameIdx);
			}
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

const GpuTexture2D& ProgressiveTerrain::getHeightmap(XMINT2 worldChunk) const
{
	int ax = TerrainGridParams::wrapIndex(worldChunk.x, GridsSize);
	int ay = TerrainGridParams::wrapIndex(worldChunk.y, GridsSize);
	return terrainGridHeight[ax][ay];
}
