#pragma once

#include "RenderCore/GpuTextureReadback.h"
#include "RenderObject/Terrain/TerrainGridParams.h"
#include "JoltHeader.h"
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <unordered_map>
#include <unordered_set>
#include <future>

class ProgressiveTerrain;
class PhysicsManager;

class TerrainPhysics
{
public:

	void init(ID3D12Device* device);

	// Called from terrain postUpdateCallback — issues GPU readback copies on the command list
	void requestReadbacks(ID3D12GraphicsCommandList* commandList, ProgressiveTerrain& terrain);

	// Called from renderFrame before compositor — consumes previous frame's readbacks (CPU only)
	void consumeReadbacks(const Vector3& cameraPos, const TerrainGridParams& params, PhysicsManager& physics);

	void clear(PhysicsManager& physics);

private:

	// Each heightmap (960px content) is divided into 4x4 physics tiles
	static constexpr UINT TilesPerHeightmap = 4;
	// 5x5 physics tiles active around camera
	static constexpr UINT PhysicsGridSize = 5;
	// Full heightmap texture size and layout
	static constexpr UINT HeightmapSize = 1024;
	static constexpr UINT HeightmapContentSize = 960;
	static constexpr UINT HeightmapBorder = 32;
	// Content pixels per physics tile
	static constexpr UINT TileContentPixels = HeightmapContentSize / TilesPerHeightmap; // 240
	// Max heightmaps needed simultaneously (5x5 tiles on 4-wide grid = max 2x2 = 4)
	static constexpr UINT MaxCachedHeightmaps = 4;

	// Physics tile size in world units
	float physicsTileSize(const TerrainGridParams& params) const { return params.tileSize / TilesPerHeightmap; }

	// --- Heightmap cache ---
	struct HeightmapCache
	{
		XMINT2 worldChunk{ INT_MAX, INT_MAX }; // which heightmap chunk this holds
		GpuTextureReadback readback;
		bool readbackPending = false;
		std::vector<uint8_t> cpuData; // persistent CPU copy after readback
		UINT rowPitch = 0;
		bool hasCpuData = false;

		bool matches(XMINT2 chunk) const { return worldChunk.x == chunk.x && worldChunk.y == chunk.y; }
		bool isValid() const { return worldChunk.x != INT_MAX; }
	};
	HeightmapCache heightmapCaches[MaxCachedHeightmaps];

	HeightmapCache* findCache(XMINT2 heightmapChunk);
	HeightmapCache* allocateCache(XMINT2 heightmapChunk);
	void ensureHeightmapReadback(ID3D12GraphicsCommandList* commandList, XMINT2 heightmapChunk, ProgressiveTerrain& terrain);

	// --- Physics tile tracking ---
	// Physics tile coordinate = finer grid (each heightmap = TilesPerHeightmap x TilesPerHeightmap tiles)
	XMINT2 physicsCenter{ INT_MAX, INT_MAX }; // in tile coordinates

	struct TileBody
	{
		JPH::BodyID bodyId;
		XMINT2 tileCoord; // physics tile world coord
	};
	static uint64_t packCoord(XMINT2 c) { return (uint64_t)(uint32_t)c.x | ((uint64_t)(uint32_t)c.y << 32); }

	std::unordered_map<uint64_t, TileBody> activeBodies;

	// Async height field building
	struct PendingBuild
	{
		XMINT2 tileCoord;
		Vector3 offset;
		std::future<JPH::BodyCreationSettings> future;
	};
	std::vector<PendingBuild> pendingBuilds;
	std::unordered_set<uint64_t> pendingBuildTiles;

	// Which heightmap chunk a physics tile belongs to
	static XMINT2 tileToHeightmapChunk(XMINT2 tileCoord);
	// Local tile index within its heightmap (0..TilesPerHeightmap-1)
	static XMINT2 tileLocalIndex(XMINT2 tileCoord);

	bool updateCenter(const Vector3& cameraPos, const TerrainGridParams& params);
	void buildTileFromCache(XMINT2 tileCoord, const TerrainGridParams& params);
	void collectFinishedBuilds(PhysicsManager& physics);
	void removeTileBody(XMINT2 tileCoord, PhysicsManager& physics);

	float hysteresisMargin = 0.15f;
	ID3D12Device* device{};
};
