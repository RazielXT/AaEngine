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

	void dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* transformBuffer, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter);
};

struct GrassChunk
{
	// Candidate blades — camera independent, generated once by grassFindCS and shared by all views.
	ComPtr<ID3D12Resource> infoBuffer;
	ComPtr<ID3D12Resource> infoCounter;

	GeometryViews<RenderViewId_ShadowCascade0, RenderViewId_ShadowCascade1> views;

	struct ViewData
	{
		ComPtr<ID3D12Resource> transformationBuffer;
		IndirectEntityGeometry indirect;
	};
	ViewData viewData[1 + decltype(views)::count];

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
	// Updating/clearing of chunks is split across frames; one division group is processed per frame.
	constexpr static UINT UpdateDivision = 2;

private:

	// Command-buffer slot layout: chunks are grouped by their update-division group so that the
	// commands processed on a given frame form a contiguous range a single clear dispatch can cover.
	// Each render view owns a TotalChunks-sized block within the shared command buffer.
	static constexpr UINT divisionGroupCount(UINT group)
	{
		return (TotalChunks - group + UpdateDivision - 1) / UpdateDivision;
	}
	static constexpr UINT divisionGroupBase(UINT group)
	{
		UINT base = 0;
		for (UINT g = 0; g < group; g++)
			base += divisionGroupCount(g);
		return base;
	}
	static constexpr UINT commandSlot(UINT viewIndex, UINT linearIndex)
	{
		return viewIndex * TotalChunks + divisionGroupBase(linearIndex % UpdateDivision) + linearIndex / UpdateDivision;
	}

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
	void createSharedCommandBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	void regenerateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk, const ProgressiveTerrain& terrain);
	void updateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk, UINT viewIndex, const GrassUpdateComputeShader::Input& cullingInput);

	ComPtr<ID3D12CommandSignature> commandSignature;
	// Single command buffer shared by all chunks and views; chunks reference their slice via offset.
	ComPtr<ID3D12Resource> sharedCommandBuffer;
	ComPtr<ID3D12Resource> zeroCounterBuffer;

	VertexBufferModel* grassModel{};
	MaterialInstance* grassMaterial{};
};
