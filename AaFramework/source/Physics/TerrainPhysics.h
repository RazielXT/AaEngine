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

	static constexpr UINT PhysicsGridSize = 3;
	static constexpr UINT HeightmapSize = 1024;
	static constexpr UINT HeightmapContentSize = 960;
	static constexpr UINT HeightmapBorder = 32;

	GpuTextureReadback readbacks[PhysicsGridSize][PhysicsGridSize];
	bool readbackPending[PhysicsGridSize][PhysicsGridSize]{};
	XMINT2 readbackWorldCoord[PhysicsGridSize][PhysicsGridSize]{};

	XMINT2 physicsCenter{ INT_MAX, INT_MAX };

	struct ChunkBody
	{
		JPH::BodyID bodyId;
		XMINT2 worldChunk;
	};

	static uint64_t packCoord(XMINT2 c) { return (uint64_t)(uint32_t)c.x | ((uint64_t)(uint32_t)c.y << 32); }

	std::unordered_map<uint64_t, ChunkBody> activeBodies;

	// Async height field building
	struct PendingBuild
	{
		XMINT2 worldChunk;
		Vector3 offset;
		std::future<JPH::BodyCreationSettings> future;
	};
	std::vector<PendingBuild> pendingBuilds;
	std::unordered_set<uint64_t> pendingBuildChunks;

	bool updateCenter(const Vector3& cameraPos, const TerrainGridParams& params);
	void requestReadback(ID3D12GraphicsCommandList* commandList, int gx, int gy, XMINT2 worldChunk, ProgressiveTerrain& terrain);
	void consumeReadback(int gx, int gy, const TerrainGridParams& params, PhysicsManager& physics);

	void startAsyncBuild(XMINT2 worldChunk, const void* readbackData, UINT rowPitch, const TerrainGridParams& params);
	void collectFinishedBuilds(PhysicsManager& physics);
	void removeChunkBody(XMINT2 worldChunk, PhysicsManager& physics);

	static int wrapIndex(int coord) { return ((coord % (int)PhysicsGridSize) + (int)PhysicsGridSize) % (int)PhysicsGridSize; }

	float hysteresisMargin = 0.15f;
	ID3D12Device* device{};
};
