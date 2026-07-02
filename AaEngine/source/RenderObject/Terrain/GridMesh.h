#pragma once

#include "Utils/MathUtils.h"
#include <vector>
#include "Resources/Shader/ShaderDataBuffers.h"
#include "Scene/RenderEntity.h"
#include "Scene/Camera.h"
#include <unordered_map>
#include "Scene/EntityGeometry.h"

// Data sent to the StructuredBuffer
struct TileData
{
	uint32_t x;   // Grid coordinate (0-15)
	uint32_t y;   // Grid coordinate (0-15)
	uint32_t lod; // Current LOD level (0-4)
	float radius;
};

class GridLODSystem
{
public:

	std::vector<TileData> m_renderList;

	UINT TilesWidth;
	UINT MaxInstances;

	void Initialize(Vector2 gridWorldSize, Vector3 gridOrigin, UINT lodsCount)
	{
		TilesWidth = (UINT)pow(2, lodsCount - 1);
		lodsLevels = lodsCount;

		// How many tiles across (etc) until next LOD transition
		const float LodLevelTilesCount = 2;

		m_gridOrigin = gridOrigin;

		// 1. Determine which side is the "base" (TilesWidth units)
		// We use the smaller side to ensure the TilesWidth x TilesWidth root fits or spans the width
		float shorterSide = min(gridWorldSize.x, gridWorldSize.y);
		m_smallestTileSize = shorterSide / TilesWidth;

		// 2. Determine how many TilesWidth-unit blocks we need for each axis
		// This implements "150% -> 2, 250% -> 3" logic via rounding
		numBlocksX = (uint32_t)std::round(gridWorldSize.x / shorterSide);
		numBlocksY = (uint32_t)std::round(gridWorldSize.y / shorterSide);

		const auto SubdivisionDetail = 1.5f;

		// 3. Pre-calculate thresholds
		// Multiplier controls how far away tiles subdivide.
		// Higher = more room for morph transitions but more tiles rendered.
		// With mult=3: morphWidth = 0.5*range, nearest vertex at removal = 1.53*range
		for (UINT i = 0; i < lodsCount; ++i) {
			float levelTileSize = (TilesWidth >> i) * m_smallestTileSize * LodLevelTilesCount;
			float limit = levelTileSize * SubdivisionDetail;
			m_subdivideThresholdsSq.push_back(limit * limit);
			m_subdivideThresholds.push_back(limit);
		}

		MaxInstances = CalculateMaxInstances(SubdivisionDetail, lodsCount);
	}

	void BuildLOD(const Vector3& cameraPos, XMINT2 offset)
	{
		m_renderList.clear();

		const float tileSize = m_smallestTileSize;
		const float worldSize = tileSize * TilesWidth;

		const float camX = cameraPos.x - offset.x * worldSize;
		const float camZ = cameraPos.z - offset.y * worldSize;

		struct Node
		{
			uint32_t x;
			uint32_t y;
			uint32_t size;
			uint32_t level;
		};
		std::vector<Node> stack;
		stack.reserve(1024);

		// Start at root blocks
		for (uint32_t bx = 0; bx < numBlocksX; ++bx)
		{
			for (uint32_t by = 0; by < numBlocksY; ++by)
			{
				stack.push_back({ bx * TilesWidth, by * TilesWidth, TilesWidth, 0 });
			}
		}

		while (!stack.empty())
		{
			Node n = stack.back();
			stack.pop_back();

			// --- Compute center ---
			const float halfSize = n.size * 0.5f;
			const float worldHalf = halfSize * tileSize;

			const float centerX = m_gridOrigin.x + (n.x * tileSize) + worldHalf;
			const float centerZ = m_gridOrigin.z + (n.y * tileSize) + worldHalf;

			// --- Distance (2D) ---
			const float dx = camX - centerX;
			const float dz = camZ - centerZ;
			const float distSq = dx * dx + dz * dz;

			// --- Subdivide? ---
			if (n.level < lodsLevels - 1 &&	distSq < m_subdivideThresholdsSq[n.level])
			{
				const uint32_t childSize = n.size >> 1;
				const uint32_t next = n.level + 1;

				stack.push_back({ n.x + childSize, n.y + childSize, childSize, next });
				stack.push_back({ n.x,             n.y + childSize, childSize, next });
				stack.push_back({ n.x + childSize, n.y,             childSize, next });
				stack.push_back({ n.x,             n.y,             childSize, next });
			}
			else
			{
				m_renderList.emplace_back(n.x, n.y,	n.level, m_subdivideThresholds[n.level]);
			}
		}
	}

	void BuildAabbLOD(const Vector3& cameraPos, XMINT2 offset, Vector2 cullMin, Vector2 cullMax)
	{
		m_renderList.clear();

		const float tileSize = m_smallestTileSize;
		const float worldSize = tileSize * TilesWidth;

		// Shift offsets to absolute world coordinates for node bounding boxes
		const float worldOffsetX = offset.x * worldSize;
		const float worldOffsetZ = offset.y * worldSize;

		const float camX = cameraPos.x - worldOffsetX;
		const float camZ = cameraPos.z - worldOffsetZ;

		struct Node
		{
			uint32_t x;
			uint32_t y;
			uint32_t size;
			uint32_t level;
		};
		std::vector<Node> stack;
		stack.reserve(1024);

		// Start at root blocks
		for (uint32_t bx = 0; bx < numBlocksX; ++bx)
		{
			for (uint32_t by = 0; by < numBlocksY; ++by)
			{
				stack.push_back({ bx * TilesWidth, by * TilesWidth, TilesWidth, 0 });
			}
		}

		while (!stack.empty())
		{
			Node n = stack.back();
			stack.pop_back();

			// --- Compute Node World AABB ---
			const float nodeMinX = m_gridOrigin.x + (n.x * tileSize) + worldOffsetX;
			const float nodeMinZ = m_gridOrigin.z + (n.y * tileSize) + worldOffsetZ;
			const float nodeMaxX = nodeMinX + (n.size * tileSize);
			const float nodeMaxZ = nodeMinZ + (n.size * tileSize);

			// --- 2D AABB Overlap Test ---
			// If the node is completely outside the culling bounds, drop it.
			if (nodeMaxX < cullMin.x || nodeMinX > cullMax.x ||
				nodeMaxZ < cullMin.y || nodeMinZ > cullMax.y)
			{
				continue;
			}

			// --- Compute center (relative to shifted camera space) ---
			const float halfSize = n.size * 0.5f;
			const float worldHalf = halfSize * tileSize;

			const float centerX = m_gridOrigin.x + (n.x * tileSize) + worldHalf;
			const float centerZ = m_gridOrigin.z + (n.y * tileSize) + worldHalf;

			// --- Distance (2D) ---
			const float dx = camX - centerX;
			const float dz = camZ - centerZ;
			const float distSq = dx * dx + dz * dz;

			// --- Subdivide? ---
			if (n.level < lodsLevels - 1 && distSq < m_subdivideThresholdsSq[n.level])
			{
				const uint32_t childSize = n.size >> 1;
				const uint32_t next = n.level + 1;

				stack.push_back({ n.x + childSize, n.y + childSize, childSize, next });
				stack.push_back({ n.x,             n.y + childSize, childSize, next });
				stack.push_back({ n.x + childSize, n.y,             childSize, next });
				stack.push_back({ n.x,             n.y,             childSize, next });
			}
			else
			{
				m_renderList.emplace_back(n.x, n.y, n.level, m_subdivideThresholds[n.level]);
			}
		}
	}

	template<typename T>
	void BuildLOD(const Vector3& cameraPos, const T& culling, XMINT2 offset, UINT lodsLevelsMax = 0)
	{
		lodsLevelsMax = lodsLevelsMax ? min(lodsLevelsMax, lodsLevels) : lodsLevels;

		m_renderList.clear();

		const float tileSize = m_smallestTileSize;
		const float worldSize = tileSize * TilesWidth;

		const float camX = cameraPos.x - offset.x * worldSize;
		const float camZ = cameraPos.z - offset.y * worldSize;

		Vector2 chunkOffset = Vector2(offset.x * worldSize, offset.y * worldSize);

		struct Node
		{
			uint32_t x;
			uint32_t y;
			uint32_t size;
			uint32_t level;
		};
		std::vector<Node> stack;
		stack.reserve(1024);

		// Start at root blocks
		for (uint32_t bx = 0; bx < numBlocksX; ++bx)
		{
			for (uint32_t by = 0; by < numBlocksY; ++by)
			{
				stack.push_back({ bx * TilesWidth, by * TilesWidth, TilesWidth, 0 });
			}
		}

		while (!stack.empty())
		{
			Node n = stack.back();
			stack.pop_back();

			// --- Compute center ---
			const float halfSize = n.size * 0.5f;
			const float worldHalf = halfSize * tileSize;

			const float centerX = m_gridOrigin.x + (n.x * tileSize) + worldHalf;
			const float centerZ = m_gridOrigin.z + (n.y * tileSize) + worldHalf;

			// --- Distance (2D) ---
			const float dx = camX - centerX;
			const float dz = camZ - centerZ;
			const float distSq = dx * dx + dz * dz;

			BoundingBox bbox;
			bbox.Center = Vector3(centerX + chunkOffset.x, 0, centerZ + chunkOffset.y);
			//bbox.Extents = Vector3(halfSize, 4000, halfSize);
			bbox.Extents = Vector3(worldHalf, 4000, worldHalf);

			// -------- CULL --------
			if (!culling.Intersects(bbox))
				continue;

			// --- Subdivide? ---
			if (n.level < lodsLevelsMax - 1 && distSq < m_subdivideThresholdsSq[n.level])
			{
				const uint32_t childSize = n.size >> 1;
				const uint32_t next = n.level + 1;

				stack.push_back({ n.x + childSize, n.y + childSize, childSize, next });
				stack.push_back({ n.x,             n.y + childSize, childSize, next });
				stack.push_back({ n.x + childSize, n.y,             childSize, next });
				stack.push_back({ n.x,             n.y,             childSize, next });
			}
			else
			{
				m_renderList.emplace_back(n.x, n.y, n.level, m_subdivideThresholds[n.level]);
			}
		}
	}

	// Representing a simple 2D bounding box
	struct Rect2D
	{
		float minX, minZ;
		float maxX, maxZ;
	};

	void BuildLODUniform(const Vector3& cameraPos, XMINT2 offset, const Rect2D& viewAABB)
	{
		m_renderList.clear();

		const float tileSize = m_smallestTileSize;
		const float chunkWorldSize = tileSize * TilesWidth;

		// Apply the offset to the camera to keep math local to this grid instance
		const float camX = cameraPos.x - offset.x * chunkWorldSize;
		const float camZ = cameraPos.z - offset.y * chunkWorldSize;

		const float chunkHalfWorld = chunkWorldSize * 0.5f;

		// Iterate over the fixed grid of uniform-sized chunks
		for (uint32_t bx = 0; bx < numBlocksX; ++bx)
		{
			for (uint32_t by = 0; by < numBlocksY; ++by)
			{
				// 1. Compute the structural Extents (Min/Max) of this chunk
				const float chunkMinX = m_gridOrigin.x + (bx * chunkWorldSize);
				const float chunkMinZ = m_gridOrigin.z + (by * chunkWorldSize);
				const float chunkMaxX = chunkMinX + chunkWorldSize;
				const float chunkMaxZ = chunkMinZ + chunkWorldSize;

				// 2. View AABB Culling Check (Separating Axis Theorem for 2D AABBs)
				// If the chunk is completely outside the view bounding box, skip it immediately.
				if (chunkMaxX < viewAABB.minX || chunkMinX > viewAABB.maxX ||
					chunkMaxZ < viewAABB.minZ || chunkMinZ > viewAABB.maxZ)
				{
					continue;
				}

				// 3. Calculate the world center (for distance/LOD calculation)
				const float centerX = chunkMinX + chunkHalfWorld;
				const float centerZ = chunkMinZ + chunkHalfWorld;

				// 4. Compute squared distance from camera to chunk center
				const float dx = camX - centerX;
				const float dz = camZ - centerZ;
				const float distSq = dx * dx + dz * dz;

				// 5. Determine LOD level based on distance thresholds
				uint32_t chosenLevel = lodsLevels - 1; // Default to worst LOD

				for (uint32_t lvl = 0; lvl < lodsLevels - 1; ++lvl)
				{
					if (distSq < m_subdivideThresholdsSq[lvl])
					{
						chosenLevel = lvl;
						break;
					}
				}

				// 6. Convert block coordinates to tile coordinates for the render list
				uint32_t tileX = bx * TilesWidth;
				uint32_t tileY = by * TilesWidth;

				m_renderList.emplace_back(tileX, tileY, chosenLevel, m_subdivideThresholds[chosenLevel]);
			}
		}
	}

private:

	uint32_t CalculateMaxInstances(float subdivisionDetail, UINT lodsCount)
	{
		uint32_t totalRootBlocks = numBlocksX * numBlocksY;
		if (lodsCount <= 1) return totalRootBlocks;

		// K is the multiplier determining how many tile-widths away subdivision happens
		float K = 2.0f * subdivisionDetail;

		// Calculate tile bounding box dimensions 
		uint32_t W_out = (uint32_t)std::ceil(4.0f * K + 1.414f);
		int32_t W_in_signed = (int32_t)std::floor(2.0f * K - 1.414f);
		uint32_t W_in = (W_in_signed > 0) ? (uint32_t)W_in_signed : 0;

		// Calculate maximum tiles per layer type
		uint32_t maxTilesPerRing = (W_out * W_out) - (W_in * W_in);
		uint32_t maxTilesCore = W_out * W_out;

		// LOD 0 handles the base world. It sheds tiles as they subdivide.
		uint32_t baseLOD0Tiles = (totalRootBlocks > (W_in * W_in)) ? (totalRootBlocks - (W_in * W_in)) : 0;
		uint32_t intermediateRingTiles = maxTilesPerRing * (lodsCount - 2);

		uint32_t totalTheoreticalMax = baseLOD0Tiles + intermediateRingTiles + maxTilesCore;

		// Apply a 15% safety buffer for floating point precision and extreme diagonal transitions
		return (uint32_t)(totalTheoreticalMax * 1.15f);
	}

	std::vector<float> m_subdivideThresholdsSq;
	std::vector<float> m_subdivideThresholds;
	float m_smallestTileSize; // The world size of a 1 / TilesWidth th tile
	Vector3 m_gridOrigin;     // World space position of tile (0,0)
	UINT lodsLevels;
	uint32_t numBlocksX;
	uint32_t numBlocksY;
};

struct GridInstanceMesh
{
	GridInstanceMesh() = default;
	~GridInstanceMesh() = default;

	void create(UINT maxInstances)
	{
		gpuBuffer = ShaderDataBuffers::get().CreateCbufferResource(maxInstances * sizeof(TileData));
	}

	void update(EntityGeometry& g, UINT count, void* data, UINT dataSize, UINT frameIdx)
	{
		g.instanceCount = count;

		auto& buf = gpuBuffer.data[frameIdx];
		g.geometryCustomBuffer = buf.GpuAddress();

		memcpy(buf.Memory(), data, dataSize);
	}

	CbufferData gpuBuffer;
};
