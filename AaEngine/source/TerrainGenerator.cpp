#include "TerrainGenerator.h"
#include "RenderSystem.h"
#include "GraphicsResources.h"
#include "SceneManager.h"

TerrainGenerator::TerrainGenerator()
{
}

void TerrainGenerator::initialize(RenderSystem& rs, GraphicsResources& resources)
{
	auto csShader = resources.shaders.getShader("TerrainGeneratorCS", ShaderTypeCompute, ShaderRef{ "terrainGenerateCS.hlsl", "main", "cs_6_6" });
	cs.init(*rs.core.device, "TerrainGeneratorCS", *csShader);

	terrainMaterial = resources.materials.getMaterial("GreenVCT");

	ResourceUploadBatch batch(rs.core.device);
	batch.Begin(D3D12_COMMAND_LIST_TYPE_COPY);

	auto file = resources.textures.loadFile(*rs.core.device, batch, "dmap.png");
	resources.descriptors.createTextureView(*file);
	heightmap = file->srvHeapIndex;

	grid.init(rs, resources, batch);

	auto uploadResourcesFinished = batch.End(rs.core.copyQueue);
	uploadResourcesFinished.wait();
}

void TerrainGenerator::createTerrain(ID3D12GraphicsCommandList* commandList, SceneManager& sceneMgr, const Vector3& position)
{
	auto createLodEntity = [&](TerrainGrid::ChunkLod& lod, std::string lodName)
		{
			for (UINT x = 0; x < 4; x++)
				for (UINT y = 0; y < 4; y++)
				{
					auto& cell = lod.data[x][y];
					if (!cell.chunk)
						continue;

					auto entity = sceneMgr.createEntity("terrain" + lodName + std::to_string(x) + std::to_string(y));
					entity->geometry.fromModel(cell.chunk->model);
					entity->material = terrainMaterial;
					entity->setBoundingBox(cell.chunk->model.bbox);
					cell.entity = entity;
				}
		};

	createLodEntity(grid.lod0, "0");
	createLodEntity(grid.lod1, "1");
	createLodEntity(grid.lod2, "2");

	generateTerrain(commandList, position);
}

void TerrainGenerator::update(ID3D12GraphicsCommandList* commandList, const Vector3& position)
{
// 	if (!rebuildScheduled)
// 		return;
// 
// 	rebuildScheduled = false;

	CommandsMarker marker(commandList, "TerrainGenerator");

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
	auto transitionToUav = [&](TerrainGrid::ChunkLod& lod)
		{
			for (UINT x = 0; x < 4; x++)
				for (UINT y = 0; y < 4; y++)
				{
					auto& cell = lod.data[x][y];
					if (!cell.chunk)
						continue;

					barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(cell.chunk->vertexBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
				}
		};

	transitionToUav(grid.lod0);
	transitionToUav(grid.lod1);
	transitionToUav(grid.lod2);

	commandList->ResourceBarrier(barriers.size(), barriers.data());

	generateTerrain(commandList, position);
}

void TerrainGenerator::rebuild()
{
	rebuildScheduled = true;
}

float snapValue(float value, float step)
{
	return round(value / step) * step;
}

void TerrainGenerator::generateTerrain(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPosition)
{
	float fullWidth = 1024.f;

	Vector2 snapPosition;
	float snapStepping = fullWidth / (64 * 4);
	snapPosition.x = snapValue(cameraPosition.x, snapStepping);
	snapPosition.y = snapValue(cameraPosition.z, snapStepping);

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

	auto updateLod = [&](TerrainGrid::ChunkLod& lod, TerrainGrid& grid, float terrainPortion, float terrainOffset)
		{
			for (UINT x = 0; x < 4; x++)
				for (UINT y = 0; y < 4; y++)
				{
					auto& cell = lod.data[x][y];
					if (!cell.chunk)
						continue;

					XMUINT4 borderSeamsIdx = { grid.ChunkSize, grid.ChunkSize, grid.ChunkSize, grid.ChunkSize };

					if (lod.borderSeamsFix)
					{
						if (x == 0)
							borderSeamsIdx.x = 0;
						if (x == 3)
							borderSeamsIdx.y = grid.ChunkSize - 1;
						if (y == 0)
							borderSeamsIdx.z = 0;
						if (y == 3)
							borderSeamsIdx.w = grid.ChunkSize - 1;
					}

					Vector2 gridOffset = Vector2(terrainOffset + x * terrainPortion, terrainOffset + y * terrainPortion);
					gridOffset.x += -0.5f + snapPosition.x / fullWidth;
					gridOffset.y += -0.5f + snapPosition.y / fullWidth;

					cs.dispatch(commandList, cell.chunk->vertexBuffer.Get(), heightmap, grid.ChunkSize, gridOffset, { x, y }, terrainPortion, borderSeamsIdx, fullWidth);
					cell.entity->setPosition({ gridOffset.x * fullWidth, -200, gridOffset.y * fullWidth });
					cell.entity->updateWorldMatrix();

					barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(cell.chunk->vertexBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
				}
		};

	const float lod2Portion = 1 / 4.f;
	const float lod2Offset = 0;

	const float lod1Portion = 1 / 8.f;
	const float lod1Offset = lod2Portion;

	const float lod0Portion = 1 / 16.f;
	const float lod0Offset = lod1Portion + lod2Portion;

	updateLod(grid.lod0, grid, lod0Portion, lod0Offset);
	updateLod(grid.lod1, grid, lod1Portion, lod1Offset);
	updateLod(grid.lod2, grid, lod2Portion, lod2Offset);

	commandList->ResourceBarrier(barriers.size(), barriers.data());
}
