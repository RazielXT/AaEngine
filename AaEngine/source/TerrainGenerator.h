#pragma once

#include "TerrainGenerationCS.h"
#include "VertexBufferModel.h"
#include "TerrainGrid.h"

class SceneManager;
class SceneEntity;
class RenderSystem;
struct GraphicsResources;
class MaterialInstance;

class TerrainGenerator
{
public:

	TerrainGenerator();

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources);

	void createTerrain(ID3D12GraphicsCommandList* commandList, SceneManager& sceneMgr, const Vector3& position);
	void update(ID3D12GraphicsCommandList* commandList, const Vector3& position);
	void rebuild();

public:

	void generateTerrain(ID3D12GraphicsCommandList* commandList, const Vector3& position);

	TerrainGenerationComputeShader cs;

	TerrainGrid grid;

	MaterialInstance* terrainMaterial{};

	UINT heightmap{};

	bool rebuildScheduled = false;
};