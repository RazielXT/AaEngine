#pragma once

#include "TerrainGenerationCS.h"
#include "VertexBufferModel.h"
#include "TerrainGrid.h"
#include "Material.h"
#include "Vegetation.h"
#include "VegetationTrees.h"

class SceneManager;
class SceneEntity;
class RenderSystem;
struct GraphicsResources;

class TerrainGenerator
{
public:

	TerrainGenerator(SceneManager& sceneMgr);

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	void createTerrain(ID3D12GraphicsCommandList* commandList, RenderSystem& rs, SceneManager& sceneMgr, GraphicsResources& resources, ResourceUploadBatch& batch);
	void update(ID3D12GraphicsCommandList* commandList, SceneManager& sceneMgr, const Vector3& position);
	void rebuild();

	VegetationTrees trees;

public:

	void generateTerrain(ID3D12GraphicsCommandList* commandList, const Vector3& position);

	TerrainGenerationComputeShader cs;

	TerrainGrid grid;

	MaterialInstance* terrainMaterial{};
	MaterialPropertiesOverrideDescription lodMatOverrides[TerrainGrid::LodsCount];

	UINT heightmap{};

	bool rebuildScheduled = false;

	Vegetation vegetation;
};