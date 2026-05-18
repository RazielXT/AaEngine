#include "TerrainPhysics.h"
#include "PhysicsManager.h"
#include "RenderObject/Terrain/ProgressiveTerrain.h"
#include "directx/d3dx12.h"
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>

using namespace JPH;

XMINT2 TerrainPhysics::tileToHeightmapChunk(XMINT2 tileCoord)
{
	// Integer floor division: tile 0..3 -> heightmap 0, tile -4..-1 -> heightmap -1
	return {
		(tileCoord.x >= 0) ? tileCoord.x / (int)TilesPerHeightmap : (tileCoord.x + 1) / (int)TilesPerHeightmap - 1,
		(tileCoord.y >= 0) ? tileCoord.y / (int)TilesPerHeightmap : (tileCoord.y + 1) / (int)TilesPerHeightmap - 1
	};
}

XMINT2 TerrainPhysics::tileLocalIndex(XMINT2 tileCoord)
{
	int lx = tileCoord.x % (int)TilesPerHeightmap;
	int ly = tileCoord.y % (int)TilesPerHeightmap;
	if (lx < 0) lx += TilesPerHeightmap;
	if (ly < 0) ly += TilesPerHeightmap;
	return { lx, ly };
}

void TerrainPhysics::init(ID3D12Device* d)
{
	device = d;
}

TerrainPhysics::HeightmapCache* TerrainPhysics::findCache(XMINT2 heightmapChunk)
{
	for (auto& cache : heightmapCaches)
		if (cache.matches(heightmapChunk))
			return &cache;
	return nullptr;
}

TerrainPhysics::HeightmapCache* TerrainPhysics::allocateCache(XMINT2 heightmapChunk)
{
	// First check if already cached
	if (auto* existing = findCache(heightmapChunk))
		return existing;

	// Find an unused slot or evict the one not needed by current 5x5 tile set
	// Determine which heightmap chunks are needed by the current physics grid
	std::unordered_set<uint64_t> neededHeightmaps;
	int halfExtent = (int)PhysicsGridSize / 2;
	for (int dx = -halfExtent; dx <= halfExtent; dx++)
		for (int dy = -halfExtent; dy <= halfExtent; dy++)
		{
			XMINT2 tile = { physicsCenter.x + dx, physicsCenter.y + dy };
			XMINT2 hc = tileToHeightmapChunk(tile);
			neededHeightmaps.insert(packCoord(hc));
		}

	// Find a slot: prefer empty, then not-needed
	HeightmapCache* bestSlot = nullptr;
	for (auto& cache : heightmapCaches)
	{
		if (!cache.isValid())
		{
			bestSlot = &cache;
			break;
		}
		if (neededHeightmaps.find(packCoord(cache.worldChunk)) == neededHeightmaps.end())
		{
			bestSlot = &cache;
			break;
		}
	}

	// Fallback: just use the first slot (shouldn't happen with correct MaxCachedHeightmaps)
	if (!bestSlot)
		bestSlot = &heightmapCaches[0];

	bestSlot->worldChunk = heightmapChunk;
	bestSlot->hasCpuData = false;
	bestSlot->readbackPending = false;
	return bestSlot;
}

void TerrainPhysics::ensureHeightmapReadback(ID3D12GraphicsCommandList* commandList, XMINT2 heightmapChunk, ProgressiveTerrain& terrain)
{
	auto* cache = allocateCache(heightmapChunk);

	// Already have CPU data or readback is already pending
	if (cache->hasCpuData || cache->readbackPending)
		return;

	if (!cache->readback.isInitialized())
		cache->readback.init(device, HeightmapSize, HeightmapSize, DXGI_FORMAT_R16_UNORM);

	auto& heightmap = terrain.getHeightmap(heightmapChunk);

	auto barrierToSrc = CD3DX12_RESOURCE_BARRIER::Transition(heightmap.texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &barrierToSrc);

	cache->readback.copyFrom(commandList, heightmap.texture.Get());

	auto barrierBack = CD3DX12_RESOURCE_BARRIER::Transition(heightmap.texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrierBack);

	cache->readbackPending = true;
}

void TerrainPhysics::requestReadbacks(ID3D12GraphicsCommandList* commandList, ProgressiveTerrain& terrain)
{
	if (physicsCenter.x == INT_MAX)
		return;

	// If any heightmap in our tile range was regenerated, invalidate its cache
	for (auto& regeneratedChunk : terrain.regeneratedChunks)
	{
		if (auto* cache = findCache(regeneratedChunk))
		{
			cache->hasCpuData = false;
			cache->readbackPending = false;
		}
	}

	// Ensure readback for all heightmaps needed by the 5x5 physics tile grid
	int halfExtent = (int)PhysicsGridSize / 2;
	std::unordered_set<uint64_t> requestedHeightmaps;
	for (int dx = -halfExtent; dx <= halfExtent; dx++)
		for (int dy = -halfExtent; dy <= halfExtent; dy++)
		{
			XMINT2 tile = { physicsCenter.x + dx, physicsCenter.y + dy };
			XMINT2 hc = tileToHeightmapChunk(tile);
			uint64_t key = packCoord(hc);
			if (requestedHeightmaps.find(key) == requestedHeightmaps.end())
			{
				requestedHeightmaps.insert(key);
				ensureHeightmapReadback(commandList, hc, terrain);
			}
		}
}

void TerrainPhysics::consumeReadbacks(const Vector3& cameraPos, const TerrainGridParams& params, PhysicsManager& physics)
{
	// Update center with hysteresis (in physics tile coordinates)
	bool centerChanged = updateCenter(cameraPos, params);

	if (centerChanged)
	{
		// Remove bodies outside the 5x5 tile grid
		int halfExtent = (int)PhysicsGridSize / 2;
		std::vector<uint64_t> toRemove;
		for (auto& [key, body] : activeBodies)
		{
			int dx = body.tileCoord.x - physicsCenter.x;
			int dy = body.tileCoord.y - physicsCenter.y;
			if (abs(dx) > halfExtent || abs(dy) > halfExtent)
				toRemove.push_back(key);
		}
		for (auto key : toRemove)
		{
			physics.removeBody(activeBodies[key].bodyId);
			activeBodies.erase(key);
		}
	}

	// Collect any finished async builds
	collectFinishedBuilds(physics);

	// Consume completed heightmap readbacks into CPU cache
	for (auto& cache : heightmapCaches)
	{
		if (cache.readbackPending)
		{
			const void* data = cache.readback.map();
			if (data)
			{
				UINT rowPitch = cache.readback.getRowPitch();
				UINT dataSize = HeightmapSize * rowPitch;
				cache.cpuData.resize(dataSize);
				memcpy(cache.cpuData.data(), data, dataSize);
				cache.rowPitch = rowPitch;
				cache.hasCpuData = true;
				cache.readback.unmap();
			}
			cache.readbackPending = false;
		}
	}

	// Build physics tiles from cached heightmap data
	int halfExtent = (int)PhysicsGridSize / 2;
	for (int dx = -halfExtent; dx <= halfExtent; dx++)
		for (int dy = -halfExtent; dy <= halfExtent; dy++)
		{
			XMINT2 tileCoord = { physicsCenter.x + dx, physicsCenter.y + dy };
			uint64_t key = packCoord(tileCoord);

			if (activeBodies.find(key) != activeBodies.end())
				continue;
			if (pendingBuildTiles.find(key) != pendingBuildTiles.end())
				continue;

			// Check if we have the heightmap data for this tile
			XMINT2 hc = tileToHeightmapChunk(tileCoord);
			auto* cache = findCache(hc);
			if (cache && cache->hasCpuData)
				buildTileFromCache(tileCoord, params);
		}
}

void TerrainPhysics::clear(PhysicsManager& physics)
{
	// Wait for any pending async builds before clearing
	for (auto& build : pendingBuilds)
	{
		if (build.future.valid())
			build.future.wait();
	}
	pendingBuilds.clear();
	pendingBuildTiles.clear();

	for (auto& [key, body] : activeBodies)
		physics.removeBody(body.bodyId);

	activeBodies.clear();
	physicsCenter = { INT_MAX, INT_MAX };

	for (auto& cache : heightmapCaches)
	{
		cache.worldChunk = { INT_MAX, INT_MAX };
		cache.hasCpuData = false;
		cache.readbackPending = false;
	}
}

bool TerrainPhysics::updateCenter(const Vector3& cameraPos, const TerrainGridParams& params)
{
	float tileSz = physicsTileSize(params);
	XMINT2 cameraTile = params.worldCoordAt(cameraPos, tileSz);

	if (physicsCenter.x == INT_MAX)
	{
		physicsCenter = cameraTile;
		return true;
	}

	float margin = tileSz * hysteresisMargin;
	Vector3 centerMin = params.chunkWorldMin(physicsCenter, tileSz);
	Vector3 centerMax = centerMin + Vector3(tileSz, 0, tileSz);

	XMINT2 newCenter = physicsCenter;
	if (cameraPos.x > centerMax.x + margin) newCenter.x++;
	else if (cameraPos.x < centerMin.x - margin) newCenter.x--;
	if (cameraPos.z > centerMax.z + margin) newCenter.y++;
	else if (cameraPos.z < centerMin.z - margin) newCenter.y--;

	if (newCenter.x != physicsCenter.x || newCenter.y != physicsCenter.y)
	{
		physicsCenter = newCenter;
		return true;
	}

	return false;
}

void TerrainPhysics::buildTileFromCache(XMINT2 tileCoord, const TerrainGridParams& params)
{
	XMINT2 hc = tileToHeightmapChunk(tileCoord);
	auto* cache = findCache(hc);
	if (!cache || !cache->hasCpuData)
		return;

	XMINT2 localIdx = tileLocalIndex(tileCoord);
	float tileSz = physicsTileSize(params);

	// Each tile samples TileContentPixels+1 pixels for TileContentPixels intervals
	// Pixel offset within the heightmap content region for this tile
	UINT tilePixelOffsetX = localIdx.x * TileContentPixels;
	UINT tilePixelOffsetY = localIdx.y * TileContentPixels;

	// Half-texel offset matching visual shader sampling
	float halfTexel = params.tileSize / (float)HeightmapContentSize * 0.5f;
	Vector3 tileWorldMin = params.chunkWorldMin(tileCoord, tileSz);
	Vector3 offset = {
		tileWorldMin.x + halfTexel,
		tileWorldMin.y,
		tileWorldMin.z + halfTexel
	};

	// Share the cached data via pointer (cache lifetime guaranteed during build)
	auto ownedData = std::make_shared<std::vector<uint8_t>>(cache->cpuData);
	UINT capturedRowPitch = cache->rowPitch;
	float tileHeight = params.tileHeight;

	auto future = std::async(std::launch::async, [ownedData, capturedRowPitch, tileSz, tileHeight, offset, tilePixelOffsetX, tilePixelOffsetY]() -> BodyCreationSettings
	{
		// Sample TileContentPixels+1 pixels = TileContentPixels intervals
		constexpr UINT physicsRes = TileContentPixels + 1; // 241
		std::vector<float> heights(physicsRes * physicsRes);

		const uint8_t* rawData = ownedData->data();

		for (UINT py = 0; py < physicsRes; py++)
		{
			for (UINT px = 0; px < physicsRes; px++)
			{
				UINT srcX = HeightmapBorder + tilePixelOffsetX + px;
				UINT srcY = HeightmapBorder + tilePixelOffsetY + py;

				const uint16_t* row = reinterpret_cast<const uint16_t*>(rawData + srcY * capturedRowPitch);
				uint16_t sample = row[srcX];

				heights[py * physicsRes + px] = static_cast<float>(sample) / 65535.0f;
			}
		}

		// 241 samples, 240 intervals covering tileSz
		float cellSize = tileSz / (float)TileContentPixels;
		Vec3 scale = { cellSize, tileHeight, cellSize };

		HeightFieldShapeSettings settings(heights.data(), Vec3(0, 0, 0), scale, physicsRes);
		auto shapeResult = settings.Create();

		if (shapeResult.HasError())
			return BodyCreationSettings();

		BodyCreationSettings creation_settings(shapeResult.Get(),
			Vec3(offset.x, offset.y, offset.z), Quat::sIdentity(),
			EMotionType::Static, 4);
		creation_settings.mFriction = 0.8f;
		creation_settings.mRestitution = 0.3f;

		return creation_settings;
	});

	pendingBuildTiles.insert(packCoord(tileCoord));
	pendingBuilds.push_back({ tileCoord, offset, std::move(future) });
}

void TerrainPhysics::collectFinishedBuilds(PhysicsManager& physics)
{
	for (auto it = pendingBuilds.begin(); it != pendingBuilds.end(); )
	{
		if (it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			auto settings = it->future.get();

			if (settings.GetShape() != nullptr)
			{
				removeTileBody(it->tileCoord, physics);

				auto bodyId = physics.system->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
				if (!bodyId.IsInvalid())
					activeBodies[packCoord(it->tileCoord)] = { bodyId, it->tileCoord };
			}

			pendingBuildTiles.erase(packCoord(it->tileCoord));
			it = pendingBuilds.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void TerrainPhysics::removeTileBody(XMINT2 tileCoord, PhysicsManager& physics)
{
	auto it = activeBodies.find(packCoord(tileCoord));
	if (it != activeBodies.end())
	{
		physics.removeBody(it->second.bodyId);
		activeBodies.erase(it);
	}
}
