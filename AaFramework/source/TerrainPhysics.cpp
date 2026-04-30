#include "TerrainPhysics.h"
#include "PhysicsManager.h"
#include "RenderObject/Terrain/ProgressiveTerrain.h"
#include "directx/d3dx12.h"
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>

using namespace JPH;

void TerrainPhysics::init(ID3D12Device* d)
{
	device = d;
}

void TerrainPhysics::requestReadbacks(ID3D12GraphicsCommandList* commandList, ProgressiveTerrain& terrain)
{
	if (physicsCenter.x == INT_MAX)
		return;

	// Request readback for chunks that were regenerated this frame and are in the physics 3x3
	for (auto& worldChunk : terrain.regeneratedChunks)
	{
		int dx = worldChunk.x - physicsCenter.x;
		int dy = worldChunk.y - physicsCenter.y;
		if (abs(dx) <= 1 && abs(dy) <= 1)
		{
			int gx = wrapIndex(worldChunk.x);
			int gy = wrapIndex(worldChunk.y);
			requestReadback(commandList, gx, gy, worldChunk, terrain);
		}
	}

	// Request readback for chunks that entered the 3x3 (from center shift) but weren't regenerated
	for (int dx = -1; dx <= 1; dx++)
		for (int dy = -1; dy <= 1; dy++)
		{
			XMINT2 worldChunk = { physicsCenter.x + dx, physicsCenter.y + dy };
			int gx = wrapIndex(worldChunk.x);
			int gy = wrapIndex(worldChunk.y);

			// If we already scheduled this chunk for readback, or it already has a body, skip
			if (readbackPending[gx][gy])
				continue;

			uint64_t key = packCoord(worldChunk);
			if (activeBodies.find(key) != activeBodies.end())
				continue;
			if (pendingBuildChunks.find(key) != pendingBuildChunks.end())
				continue;

			requestReadback(commandList, gx, gy, worldChunk, terrain);
		}
}

void TerrainPhysics::consumeReadbacks(const Vector3& cameraPos, const TerrainGridParams& params, PhysicsManager& physics)
{
	// Update center with hysteresis
	bool centerChanged = updateCenter(cameraPos, params);

	if (centerChanged)
	{
		// Remove bodies that are no longer in the 3x3
		std::vector<uint64_t> toRemove;
		for (auto& [key, body] : activeBodies)
		{
			int dx = body.worldChunk.x - physicsCenter.x;
			int dy = body.worldChunk.y - physicsCenter.y;
			if (abs(dx) > 1 || abs(dy) > 1)
				toRemove.push_back(key);
		}
		for (auto key : toRemove)
		{
			physics.removeBody(activeBodies[key].bodyId);
			activeBodies.erase(key);
		}
	}

	// Collect any finished async builds (adds bodies to physics system)
	collectFinishedBuilds(physics);

	// Consume completed readbacks from previous frame — launches async builds
	for (int gx = 0; gx < (int)PhysicsGridSize; gx++)
		for (int gy = 0; gy < (int)PhysicsGridSize; gy++)
		{
			if (readbackPending[gx][gy])
				consumeReadback(gx, gy, params, physics);
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
	pendingBuildChunks.clear();

	for (auto& [key, body] : activeBodies)
		physics.removeBody(body.bodyId);

	activeBodies.clear();
	physicsCenter = { INT_MAX, INT_MAX };

	for (int gx = 0; gx < (int)PhysicsGridSize; gx++)
		for (int gy = 0; gy < (int)PhysicsGridSize; gy++)
			readbackPending[gx][gy] = false;
}

bool TerrainPhysics::updateCenter(const Vector3& cameraPos, const TerrainGridParams& params)
{
	XMINT2 cameraChunk = params.worldCoordAt(cameraPos, params.tileSize);

	if (physicsCenter.x == INT_MAX)
	{
		physicsCenter = cameraChunk;
		return true;
	}

	float margin = params.tileSize * hysteresisMargin;
	Vector3 centerMin = params.chunkWorldMin(physicsCenter, params.tileSize);
	Vector3 centerMax = centerMin + Vector3(params.tileSize, 0, params.tileSize);

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

void TerrainPhysics::requestReadback(ID3D12GraphicsCommandList* commandList, int gx, int gy, XMINT2 worldChunk, ProgressiveTerrain& terrain)
{
	if (!readbacks[gx][gy].isInitialized())
		readbacks[gx][gy].init(device, HeightmapSize, HeightmapSize, DXGI_FORMAT_R16_UNORM);

	auto& heightmap = terrain.getHeightmap(worldChunk);

	auto barrierToSrc = CD3DX12_RESOURCE_BARRIER::Transition(heightmap.texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &barrierToSrc);

	readbacks[gx][gy].copyFrom(commandList, heightmap.texture.Get());

	auto barrierBack = CD3DX12_RESOURCE_BARRIER::Transition(heightmap.texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrierBack);

	readbackPending[gx][gy] = true;
	readbackWorldCoord[gx][gy] = worldChunk;
}

void TerrainPhysics::consumeReadback(int gx, int gy, const TerrainGridParams& params, PhysicsManager& physics)
{
	readbackPending[gx][gy] = false;

	const void* data = readbacks[gx][gy].map();
	if (data)
	{
		// Remove existing body before starting async rebuild
		removeChunkBody(readbackWorldCoord[gx][gy], physics);
		startAsyncBuild(readbackWorldCoord[gx][gy], data, readbacks[gx][gy].getRowPitch(), params);
		readbacks[gx][gy].unmap();
	}
}

void TerrainPhysics::startAsyncBuild(XMINT2 worldChunk, const void* readbackData, UINT rowPitch, const TerrainGridParams& params)
{
	// Copy raw heightmap data so the readback buffer can be reused
	const UINT dataSize = HeightmapSize * rowPitch;
	auto ownedData = std::make_shared<std::vector<uint8_t>>(dataSize);
	memcpy(ownedData->data(), readbackData, dataSize);

	float borderWorldOffset = (float)HeightmapBorder / (float)HeightmapSize * params.tileSize;
	Vector3 chunkMin = params.chunkWorldMin(worldChunk, params.tileSize);
	Vector3 offset = {
		chunkMin.x + borderWorldOffset,
		chunkMin.y,
		chunkMin.z + borderWorldOffset
	};

	float tileSize = params.tileSize;
	float tileHeight = params.tileHeight;
	UINT capturedRowPitch = rowPitch;

	auto future = std::async(std::launch::async, [ownedData, capturedRowPitch, tileSize, tileHeight, offset]() -> BodyCreationSettings
	{
		constexpr UINT physicsRes = 480;
		std::vector<float> heights(physicsRes * physicsRes);

		const uint8_t* rawData = ownedData->data();

		for (UINT py = 0; py < physicsRes; py++)
		{
			for (UINT px = 0; px < physicsRes; px++)
			{
				UINT srcX = HeightmapBorder + px * 2;
				UINT srcY = HeightmapBorder + py * 2;

				const uint16_t* row = reinterpret_cast<const uint16_t*>(rawData + srcY * capturedRowPitch);
				uint16_t sample = row[srcX];

				heights[py * physicsRes + px] = static_cast<float>(sample) / 65535.0f;
			}
		}

		float cellSize = tileSize / (float)physicsRes;
		Vec3 scale = { cellSize, tileHeight, cellSize };

		HeightFieldShapeSettings settings(heights.data(), Vec3(0, 0, 0), scale, physicsRes);
		auto shapeResult = settings.Create();

		if (shapeResult.HasError())
			return BodyCreationSettings();

		BodyCreationSettings creation_settings(shapeResult.Get(),
			Vec3(offset.x, offset.y, offset.z), Quat::sIdentity(),
			EMotionType::Static, 4);
		creation_settings.mFriction = 0.2f;
		creation_settings.mRestitution = 0.3f;

		return creation_settings;
	});

	pendingBuildChunks.insert(packCoord(worldChunk));
	pendingBuilds.push_back({ worldChunk, offset, std::move(future) });
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
				// Remove any stale body before adding the new one
				removeChunkBody(it->worldChunk, physics);

				auto bodyId = physics.system->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
				if (!bodyId.IsInvalid())
					activeBodies[packCoord(it->worldChunk)] = { bodyId, it->worldChunk };
			}

			pendingBuildChunks.erase(packCoord(it->worldChunk));
			it = pendingBuilds.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void TerrainPhysics::removeChunkBody(XMINT2 worldChunk, PhysicsManager& physics)
{
	auto it = activeBodies.find(packCoord(worldChunk));
	if (it != activeBodies.end())
	{
		physics.removeBody(it->second.bodyId);
		activeBodies.erase(it);
	}
}
