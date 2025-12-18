#include "TerrainGenerator.h"
#include "RenderSystem.h"
#include "GraphicsResources.h"
#include "SceneManager.h"

TerrainGenerator::TerrainGenerator(SceneManager& sceneMgr) : trees(sceneMgr)
{
}

constexpr UINT MaxLodScale_Quads = constexpr_pow(2, TerrainGrid::LodsCount - 1);
constexpr UINT StepLodScale_Quads = constexpr_pow(2, 4); //lod4

constexpr UINT ChunkWidth_Quads = TerrainGrid::ChunkWidthQuads;
constexpr UINT GridWidth_Chunks = 4;
constexpr UINT GridHalfWidth_Chunks = 2;
constexpr UINT WorldGridWidth_Quads = GridWidth_Chunks * MaxLodScale_Quads * ChunkWidth_Quads;

constexpr float QuadSize_Units = 4.f; //this controls tessellation detail
constexpr float WorldMappingScale = QuadSize_Units; //this controls heightmap mapping
constexpr float WorldWidth_Units = QuadSize_Units * WorldGridWidth_Quads;
constexpr float WorldHeight_Units = 8000.f;
constexpr float WorldWidthPerMap_Units = WorldWidth_Units / WorldMappingScale;

void TerrainGenerator::initialize(RenderSystem& rs, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	auto csShader = resources.shaders.getShader("TerrainGeneratorCS", ShaderType::Compute, ShaderRef{ "terrainGenerateCS.hlsl", "main", "cs_6_6" });
	cs.init(*rs.core.device, *csShader);

	terrainMaterial = resources.materials.getMaterial("TerrainVCT");

	auto file = resources.textures.loadFile(*rs.core.device, batch, "Mountain Range Height Map PNG.png");
	resources.descriptors.createTextureView(*file);
	heightmap = file->srvHeapIndex;

	grid.init(rs, resources, batch, WorldWidth_Units, WorldHeight_Units);

	vegetation.initialize(rs, resources, batch);

	trees.initialize(resources, batch);
}

void TerrainGenerator::createTerrain(ID3D12GraphicsCommandList* commandList, RenderSystem& rs, SceneManager& sceneMgr, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	constexpr float GridHalfWidthLod0_Units = ChunkWidth_Quads * GridHalfWidth_Chunks * QuadSize_Units;
	constexpr float GridBlendOffset_Units = QuadSize_Units * StepLodScale_Quads;

	float halfWidthLod = GridHalfWidthLod0_Units;
	for (auto& m : lodMatOverrides)
	{
		m.params.push_back({ "BlendDistance", sizeof(float), { halfWidthLod - GridBlendOffset_Units } });
		halfWidthLod *= 2;
	}

	auto createLodEntity = [&](TerrainGrid::ChunkLod& lod, UINT idx)
		{
			for (UINT x = 0; x < 4; x++)
				for (UINT y = 0; y < 4; y++)
				{
					auto& cell = lod.data[x][y];
					if (!cell.chunk)
						continue;

					auto entity = sceneMgr.createEntity("terrain" + std::to_string(idx) + std::to_string(x) + std::to_string(y));
					entity->geometry.fromModel(cell.chunk->model);
					entity->material = terrainMaterial;
					entity->setBoundingBox(cell.chunk->model.bbox);
					entity->materialOverride = &lodMatOverrides[idx];
					cell.entity = entity;
				}
		};

	for (UINT idx = 0; auto& lod : grid.lods)
		createLodEntity(lod, idx++);

	vegetation.createDrawObject(sceneMgr, rs, *resources.materials.getMaterial("Billboard", batch), batch, resources);
}

void TerrainGenerator::update(ID3D12GraphicsCommandList* commandList, SceneManager& sceneMgr, const Vector3& position)
{
// 	if (!rebuildScheduled)
// 		return;
// 
// 	rebuildScheduled = false;

	CommandsMarker marker(commandList, "TerrainGenerator", PixColor::Terrain);

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

	for (auto& lod : grid.lods)
		transitionToUav(lod);

	commandList->ResourceBarrier(barriers.size(), barriers.data());

	generateTerrain(commandList, position);

	vegetation.update(commandList, heightmap);

	trees.update(position);
}

void TerrainGenerator::rebuild()
{
	rebuildScheduled = true;
}

void TerrainGenerator::generateTerrain(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPosition)
{
	constexpr UINT QuadsPerStep = 2; //we need to step by 2 quads to maintain topology |\|/|
	constexpr UINT StepQuadsCount = StepLodScale_Quads * QuadsPerStep; //biggest lod quad
	constexpr float StepWidth_Units = StepQuadsCount * QuadSize_Units; //in world units

	Vector2 cameraOffset = { cameraPosition.x + StepWidth_Units * 0.5f, cameraPosition.z + StepWidth_Units * 0.5f }; //center by half offset
	if (cameraOffset.x < 0)
		cameraOffset.x -= StepWidth_Units;
	if (cameraOffset.y < 0)
		cameraOffset.y -= StepWidth_Units;

	XMINT2 snapPosition{};
	snapPosition.x = (int)(cameraOffset.x / StepWidth_Units) * StepQuadsCount;
	snapPosition.y = (int)(cameraOffset.y / StepWidth_Units) * StepQuadsCount;

	// ---------------------

	constexpr UINT MaxStepQuadsCount = MaxLodScale_Quads * QuadsPerStep;
	constexpr float MaxStepWidth_Units = MaxStepQuadsCount * QuadSize_Units;

	Vector2 cameraOffsetMax = { cameraPosition.x + MaxStepWidth_Units * 0.5f, cameraPosition.z + MaxStepWidth_Units * 0.5f };
	if (cameraOffsetMax.x < 0)
		cameraOffsetMax.x -= MaxStepWidth_Units;
	if (cameraOffsetMax.y < 0)
		cameraOffsetMax.y -= MaxStepWidth_Units;

	XMINT2 snapPositionMax{};
	snapPositionMax.x = (int)(cameraOffsetMax.x / MaxStepWidth_Units) * MaxStepQuadsCount;
	snapPositionMax.y = (int)(cameraOffsetMax.y / MaxStepWidth_Units) * MaxStepQuadsCount;

	// ---------------------

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
	barriers.reserve(grid.LodsCount * 12 + 4);

	auto updateLod = [&](TerrainGrid::ChunkLod& lod, TerrainGrid& grid, UINT gridScale, const XMINT2& snapPosition)
		{
			for (UINT x = 0; x < GridWidth_Chunks; x++)
				for (UINT y = 0; y < GridWidth_Chunks; y++)
				{
					auto& cell = lod.data[x][y];
					if (!cell.chunk)
						continue;

					XMUINT4 borderSeamsIdx = { grid.ChunkWidthVertices, grid.ChunkWidthVertices, grid.ChunkWidthVertices, grid.ChunkWidthVertices };

					if (lod.fixBorderSeams)
					{
						if (x == 0)
							borderSeamsIdx.x = 0;
						if (x == 3)
							borderSeamsIdx.y = ChunkWidth_Quads;
						if (y == 0)
							borderSeamsIdx.z = 0;
						if (y == 3)
							borderSeamsIdx.w = ChunkWidth_Quads;
					}

					const UINT gridStepSize = ChunkWidth_Quads * gridScale;

					XMINT2 gridOffset(x * gridStepSize, y * gridStepSize);
					//center, move grid by half
					gridOffset.x -= gridStepSize * GridHalfWidth_Chunks;
					gridOffset.y -= gridStepSize * GridHalfWidth_Chunks;
					gridOffset.x += snapPosition.x;
					gridOffset.y += snapPosition.y;

					cs.dispatch(commandList, cell.chunk->vertexBuffer.Get(), heightmap, WorldGridWidth_Quads, gridOffset, grid.ChunkWidthVertices, gridScale, borderSeamsIdx, QuadSize_Units, WorldHeight_Units, WorldMappingScale);
					cell.entity->setPosition({ gridOffset.x * WorldWidth_Units / WorldGridWidth_Quads, WorldHeight_Units / -2.f, gridOffset.y * WorldWidth_Units / WorldGridWidth_Quads });
					cell.entity->updateWorldMatrix();

					barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(cell.chunk->vertexBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
				}
		};

	for (UINT idx = 0; auto& lod : grid.lods)
	{
		const UINT lodScale = std::pow(2, idx++);
		updateLod(lod, grid, lodScale, lodScale > StepLodScale_Quads ? snapPositionMax : snapPosition);
	}

	commandList->ResourceBarrier(barriers.size(), barriers.data());
}
