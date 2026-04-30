#pragma once

#include "RenderCore/RenderSystem.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Compute/ComputeShader.h"
#include "Scene/EntityGeometry.h"
#include "RenderObject/Terrain/TerrainGridParams.h"

class SceneManager;
class ProgressiveTerrain;

class GrassFindComputeShader : public ComputeShader
{
public:

	struct Input
	{
		UINT terrainTexture;
		float terrainHeight;
		Vector2 terrainOffset;
		Vector2 chunkWorldSize;
		Vector2 subUvOffset;
		Vector2 subUvScale;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* counter);
};

class GrassUpdateComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* transformBuffer, ID3D12Resource* commands, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter);
};

struct GrassInfo
{
	Vector3 position;
	float rotation;
	float scale;
	float random;
};

struct GrassChunk
{
	ComPtr<ID3D12Resource> infoBuffer;
	ComPtr<ID3D12Resource> transformationBuffer;
	ComPtr<ID3D12Resource> infoCounter;
	IndirectEntityGeometry indirect;
	SceneEntity* entity{};

	XMINT2 worldCoord{};
	bool dirty = true;
	bool firstRun = true;
};

class Grass
{
public:

	Grass();

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	void createChunks(SceneManager& sceneMgr, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	void clear();

	void update(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPos, const ProgressiveTerrain& terrain);

	// Each grass chunk = 1 terrain tile / ChunksPerTerrainTile, so chunk aligns to a fraction of the heightmap
	constexpr static UINT ChunksPerTerrainTile = 32;
	constexpr static UINT GrassGridSize = 6;

private:

	// Find CS dispatch dimensions — smaller than vegetation for denser coverage per chunk
	static constexpr UINT groups = 4;
	static constexpr UINT threads = 8;
	static constexpr UINT perThread = 4;
	static constexpr UINT maxPerChunk = (groups * threads * perThread) * (groups * threads * perThread);

	GrassChunk chunks[GrassGridSize][GrassGridSize];
	XMINT2 gridCenterChunk = { 0, 0 };

	GrassFindComputeShader grassFindCS;
	GrassUpdateComputeShader grassUpdateCS;

	void initChunk(GrassChunk& chunk, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);
	void regenerateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk, const ProgressiveTerrain& terrain);
	void updateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk);

	ComPtr<ID3D12CommandSignature> commandSignature;
	ComPtr<ID3D12Resource> defaultCommandBuffer;
	ComPtr<ID3D12Resource> zeroCounterBuffer;

	VertexBufferModel* grassModel{};
	MaterialInstance* grassMaterial{};
};
