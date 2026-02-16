#include "ProgressiveTerrain.h"
#include "TextureResources.h"
#include "GraphicsResources.h"
#include "SceneManager.h"
#include <format>

const UINT TextureSize = 1024;
const UINT TerrainModelSize = 33;

ProgressiveTerrain::ProgressiveTerrain()
{
}

void ProgressiveTerrain::initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& sceneMgr)
{
	terrainTexture = resources.textures.loadFile(*renderSystem.core.device, batch, "Mountain Range Height Map PNG.png"); //"Mountain Range Height Map PNG.png");
	resources.descriptors.createTextureView(*terrainTexture);

	float gridTileSize = 8000.f;
	float gridTileHeight = 8000.f;
	Vector3 terrainCenterPosition = { gridTileSize * -0.5f, gridTileHeight * -0.5f, gridTileSize * -0.5f };

	terrainGridTiles.Initialize({ gridTileSize, gridTileSize }, terrainCenterPosition, 10);

	terrainModel.CreateIndexBufferGrid(renderSystem.core.device, &batch, TerrainModelSize);
	terrainModel.bbox.Extents = { gridTileSize * 0.5f, gridTileHeight * 0.5f, gridTileSize * 0.5f };
	terrainModel.bbox.Center = { gridTileSize * 0.5f, gridTileHeight * 0.5f, gridTileSize * 0.5f };

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

			auto e = sceneMgr.createEntity("Terrain" + std::format("{}{}",x,y), Order::Normal);
			terrainGridMesh[x][y].create(terrainGridTiles.TilesWidth);
			terrainGridMesh[x][y].entity = e;
			e->geometry.fromInstancedModel(terrainModel, 0, terrainGridMesh[x][y].gpuBuffer.data[0].GpuAddress());
			e->setBoundingBox(terrainModel.bbox);
			e->material = resources.materials.getMaterial("DarkGreenGrid", batch);
			e->Material().setParam("TexIdHeightmap", terrainHeight.view.srvHeapIndex);
			e->Material().setParam("TexIdNormalmap", terrainNormal.view.srvHeapIndex);
			auto GridHeightWidth = Vector2(gridTileHeight, gridTileSize);
			e->Material().setParam("GridHeightWidth", &GridHeightWidth, sizeof(GridHeightWidth));

			Vector3 pos = terrainCenterPosition;
			pos.x += (x - 2) * gridTileSize;
			pos.z += (y - 2) * gridTileSize;
			e->setPosition(pos);
		}


	struct GridInstanceInfo
	{
		Vector3 WorldPosition;
		Vector2 WorldSize;
	}
	grids[2] = { { Vector3(0,0,0), Vector2(100,100) }, { Vector3(0,100,0), Vector2(200,200) } };

	static auto gpuBuffer = ShaderDataBuffers::get().CreateCbufferResource(sizeof(grids));
	for (auto& buf : gpuBuffer.data)
		memcpy(buf.Memory(), grids, sizeof(grids));

	auto e = sceneMgr.createEntity("TerrainMesh", Order::Normal);
	e->geometry.fromMeshInstancedModel(2, gpuBuffer.data[0].GpuAddress());
	e->setBoundingBox(terrainModel.bbox);
	e->material = resources.materials.getMaterial("TerrainMesh", batch);
	e->Material().setParam("TexIdHeightmap", terrainGridHeight[0][0].view.srvHeapIndex);
	e->Material().setParam("TexIdNormalmap", terrainGridNormal[0][0].view.srvHeapIndex);
	e->setPosition({ 0, 100, 0 });

	auto csShader = resources.shaders.getShader("generateHeightmapNormalsCS", ShaderType::Compute, ShaderRef{ "grid/generateHeightmapNormalsCS.hlsl", "CSMain", "cs_6_6" });
	heightmapToNormalCS.init(*renderSystem.core.device, *csShader);

	generateNormalMipsCS.init(*renderSystem.core.device, "generateNormalMipmaps4x", resources.shaders);

	csShader = resources.shaders.getShader("generateHeightmapCS", ShaderType::Compute, ShaderRef{ "terrain/terrainGridHeightmapCS.hlsl", "CSMain", "cs_6_6" });
	generateHeightmapCS.init(*renderSystem.core.device, *csShader);

	updateTerrain = true;
}

void ProgressiveTerrain::update(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPos, UINT frameIdx)
{
	if (updateTerrain)
	for (int x = 0; x < GridsSize; x++)
		for (int y = 0; y < GridsSize; y++)
		{
			auto& terrainHeight = terrainGridHeight[x][y];
			auto& terrainNormal = terrainGridNormal[x][y];
			auto& terrainNormalMips = terrainGridNormalMips[x][y];

			TextureTransitions<3> tr;
			tr.add(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			tr.add(&terrainNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			tr.add(&terrainHeight, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			tr.push(commandList);

			generateHeightmapCS.dispatch(commandList, TextureSize, TextureSize, terrainHeight.view.uavHandle, { {x - 2,y - 2}, terrainTexture->srvHeapIndex });

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

	updateTerrain = false;

	if (updateLod)
	{
		for (int x = 0; x < GridsSize; x++)
			for (int y = 0; y < GridsSize; y++)
			{
				terrainGridTiles.BuildLOD(cameraPos, { x - 2, y - 2 });
				terrainGridMesh[x][y].update((UINT)terrainGridTiles.m_renderList.size(), terrainGridTiles.m_renderList.data(), (UINT)terrainGridTiles.m_renderList.size() * sizeof(TileData), frameIdx);
			}
	}
}