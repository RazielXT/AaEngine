#pragma once

#include "MathUtils.h"
#include <vector>
#include "ShaderDataBuffers.h"
#include "SceneEntity.h"

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

		// 3. Pre-calculate thresholds
		for (UINT i = 0; i < lodsCount; ++i) {
			float levelTileSize = (TilesWidth >> i) * m_smallestTileSize * LodLevelTilesCount;
			float limit = levelTileSize * 1.25f;
			m_subdivideThresholdsSq.push_back(limit * limit);
			m_subdivideThresholds.push_back(limit);
		}
	}

	void BuildLOD(const Vector3& cameraPos, XMINT2 offset)
	{
		m_renderList.clear();

		auto worldSize = m_smallestTileSize * TilesWidth;
		Vector3 cameraPosCentered = cameraPos - Vector3(offset.x * worldSize, 0, offset.y * worldSize);

		// Iterate through all Root Blocks
		for (uint32_t bx = 0; bx < numBlocksX; ++bx) {
			for (uint32_t by = 0; by < numBlocksY; ++by) {
				// Each root is a TilesWidth x TilesWidth unit block
				RecursiveSubdivide(bx * TilesWidth, by * TilesWidth, TilesWidth, 0, cameraPosCentered);
			}
		}
	}

private:

	void RecursiveSubdivide(uint32_t x, uint32_t y, uint32_t size, uint32_t level, const Vector3& cameraPos)
	{
		// Center calculation in world space
		float worldHalfSize = (size * 0.5f) * m_smallestTileSize;
		Vector3 tileCenterWorld = m_gridOrigin + Vector3(
			(x * m_smallestTileSize) + worldHalfSize,
			0, // Update this if your grid origin.y is the floor height
			(y * m_smallestTileSize) + worldHalfSize
		);

		// Full Vector3 distance check (handles camera height/depth)
		float distSq = Vector3::DistanceSquared(Vector3(cameraPos.x, m_gridOrigin.y, cameraPos.z), tileCenterWorld);

		if (level < (lodsLevels - 1) && distSq < m_subdivideThresholdsSq[level]) {
			uint32_t childSize = size / 2;
			uint32_t nextLod = level + 1;

			RecursiveSubdivide(x, y, childSize, nextLod, cameraPos);
			RecursiveSubdivide(x + childSize, y, childSize, nextLod, cameraPos);
			RecursiveSubdivide(x, y + childSize, childSize, nextLod, cameraPos);
			RecursiveSubdivide(x + childSize, y + childSize, childSize, nextLod, cameraPos);
		}
		else {
			// Output as integer coordinates and LOD
			m_renderList.push_back({ x, y, level, m_subdivideThresholds[level] });
		}
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

	void create(UINT tilesResolution)
	{
		gpuBuffer = ShaderDataBuffers::get().CreateCbufferResource(tilesResolution * tilesResolution * sizeof(TileData));
	}

	void update(UINT count, void* data, UINT dataSize, UINT frameIdx)
	{
		auto& g = entity->geometry;
		g.instanceCount = count;

		auto& buf = gpuBuffer.data[frameIdx];
		g.geometryCustomBuffer = buf.GpuAddress();

		memcpy(buf.Memory(), data, dataSize);
	}

	CbufferData gpuBuffer;

	SceneEntity* entity{};
	BoundingBox modelBbox;
};
