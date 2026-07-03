#pragma once

#include "RenderCore/RenderSystem.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Compute/ComputeShader.h"
#include "Resources/Compute/IndirectCommandsCS.h"
#include "Scene/EntityGeometry.h"
#include "RenderObject/Terrain/TerrainGridParams.h"

class RenderWorld;
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
		UINT tileId;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* counter);
};

// Realigns the shared candidate buffer after tiles move: streams the source buffer into the
// destination, dropping every instance whose tile id is flagged in dirtyMask (those tiles are
// regenerated afterwards by the find shader appending to the compacted buffer).
class GrassCompactComputeShader : public ComputeShader
{
public:

	struct Input
	{
		UINT dirtyMask;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* srcInfoBuffer, ID3D12Resource* srcCounter, ID3D12Resource* dstInfoBuffer, ID3D12Resource* dstCounter, UINT dispatchGroupsX);
};

class GrassUpdateComputeShader : public ComputeShader
{
public:

	struct Input
	{
		XMFLOAT4 frustumPlanes[6];
		XMFLOAT3 cameraPosition;
		float maxDistance;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* transformBuffer, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter, UINT dispatchGroupsX);
};

// Logical grass tile: tracks where in the world it currently sits. All tiles share one candidate
// buffer; a tile only marks itself dirty when it moves so it gets regenerated and re-appended.
struct GrassTile
{
	XMINT2 worldCoord{};
	bool dirty = true;
};

class Grass
{
public:

	Grass();

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	void createChunks(RenderWorld& renderWorld, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	void clear();
	void enableUpdating(bool enabled);

	void update(ID3D12GraphicsCommandList* commandList, const Camera& camera, const ProgressiveTerrain& terrain);
	// Culls grass for a single render view. cullCamera supplies the frustum; distanceCamera supplies
	// the position used for distance fade (always the main camera, even for shadow cascades).
	void updateCulling(ID3D12GraphicsCommandList* commandList, const Camera& cullCamera, const Camera& distanceCamera, const ProgressiveTerrain& terrain, UINT viewIndex);

	// Each grass chunk = 1 terrain tile / ChunksPerTerrainTile, so chunk aligns to a fraction of the heightmap
	constexpr static UINT ChunksPerTerrainTile = 32;
	constexpr static UINT GrassGridSize = 5;
	constexpr static UINT TotalChunks = GrassGridSize * GrassGridSize;

private:

	// Find CS dispatch dimensions — smaller than vegetation for denser coverage per chunk
	static constexpr UINT groups = 4;
	static constexpr UINT threads = 8;
	static constexpr UINT perThread = 4;
	static constexpr UINT maxPerChunk = (groups * threads * perThread) * (groups * threads * perThread);

	// Worst-case candidate count once every tile is fully populated; sizes the shared buffers.
	static constexpr UINT MaxTotalInstances = maxPerChunk * TotalChunks;

	// Views that share the single grass entity: view 0 = main camera (entity.geometry), the rest map
	// to shadow cascades through the viewVariants table.
	using GrassGeometryViews = GeometryViews<RenderViewId_ShadowCascade0, RenderViewId_ShadowCascade1>;
	static constexpr UINT ViewCount = 1 + GrassGeometryViews::count;

	// Compaction and culling both stream the whole candidate buffer; the compute index formula packs
	// 64x64 items per group in X, so this many X groups covers MaxTotalInstances.
	static constexpr UINT DispatchGroupsX = (MaxTotalInstances + (64 * 64 - 1)) / (64 * 64);

	GrassTile tiles[GrassGridSize][GrassGridSize];
	XMINT2 gridCenterChunk = { 0, 0 };

	GrassFindComputeShader grassFindCS;
	GrassCompactComputeShader grassCompactCS;
	GrassUpdateComputeShader grassUpdateCS;
	IndirectDrawIndexedClearCS indirectDrawClearCS;
	bool updatingEnabled = true;

	void createSharedBuffers(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);
	void createSharedCommandBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	void regenerateTiles(ID3D12GraphicsCommandList* commandList, UINT dirtyMask, const ProgressiveTerrain& terrain);
	GrassFindComputeShader::Input buildFindInput(const GrassTile& tile, UINT tileId, const ProgressiveTerrain& terrain) const;

	// Ping-pong candidate buffers (single logical stream). currentInfo indexes the live buffer; the
	// other is the compaction scratch. Both rest in NON_PIXEL_SHADER_RESOURCE state.
	ComPtr<ID3D12Resource> infoBuffer[2];
	ComPtr<ID3D12Resource> infoCounter[2];
	UINT currentInfo = 0;

	// Per-view compacted visible instances written by the culling pass.
	ComPtr<ID3D12Resource> transformationBuffer[ViewCount];

	RenderEntity* entity{};
	GrassGeometryViews views;
	IndirectEntityGeometry indirect[ViewCount];

	ComPtr<ID3D12CommandSignature> commandSignature;
	// One draw command per view (all tiles batched into a single indirect draw per view).
	ComPtr<ID3D12Resource> sharedCommandBuffer;
	ComPtr<ID3D12Resource> zeroCounterBuffer;

	VertexBufferModel* grassModel{};
	MaterialInstance* grassMaterial{};
};
