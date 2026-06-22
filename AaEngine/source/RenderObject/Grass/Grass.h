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
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* counter);
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

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* transformBuffer, ID3D12Resource* commands, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter);
};

struct GrassChunk
{
	// Candidate blades — camera independent, generated once by grassFindCS and shared by all views.
	ComPtr<ID3D12Resource> infoBuffer;
	ComPtr<ID3D12Resource> infoCounter;

	// Per-view culled data (view 0 = main camera, 1.. = shadow cascades).
	struct ViewData
	{
		ComPtr<ID3D12Resource> transformationBuffer;
		IndirectEntityGeometry indirect;
	};
	ViewData views[GeometryViewType_Count];
	GeometryViewVariants geometryVariants;
	static constexpr GeometryViewType grassGeometryViews[3] = { GeometryViewType_Default, GeometryViewType_ShadowCascade0, GeometryViewType_ShadowCascade1 };

	RenderEntity* entity{};

	XMINT2 worldCoord{};
	bool dirty = true;
	bool firstRun = true;
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

	GrassChunk chunks[GrassGridSize][GrassGridSize];
	XMINT2 gridCenterChunk = { 0, 0 };

	GrassFindComputeShader grassFindCS;
	GrassUpdateComputeShader grassUpdateCS;
	IndirectDrawIndexedClearCS indirectDrawClearCS;
	bool updatingEnabled = true;

	void initChunk(GrassChunk& chunk, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);
	void regenerateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk, const ProgressiveTerrain& terrain);
	void updateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk, UINT viewIndex, const GrassUpdateComputeShader::Input& cullingInput);

	ComPtr<ID3D12CommandSignature> commandSignature;
	ComPtr<ID3D12Resource> zeroCounterBuffer;

	VertexBufferModel* grassModel{};
	MaterialInstance* grassMaterial{};
};
