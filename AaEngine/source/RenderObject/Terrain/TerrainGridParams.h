#pragma once

#include "Utils/MathUtils.h"

struct TerrainGridParams
{
	float tileSize = 8000.f;
	float tileHeight = 8000.f;
	Vector3 tileCenterOffset;

	constexpr static UINT GridsSize = 5;

	static int wrapIndex(int coord, int size)
	{
		return ((coord % size) + size) % size;
	}

	// World-space min corner of a chunk at worldCoord with given chunkSize.
	// The chunk covers [min, min + chunkSize] in X and Z, [min.y, min.y + tileHeight] in Y.
	// Entity position = chunkWorldMin when using bbox with center = extents = size/2.
	Vector3 chunkWorldMin(XMINT2 worldCoord, float chunkSize) const
	{
		return {
			tileCenterOffset.x + worldCoord.x * chunkSize,
			tileCenterOffset.y,
			tileCenterOffset.z + worldCoord.y * chunkSize
		};
	}

	// Which worldCoord chunk contains the given world position for a given chunkSize.
	XMINT2 worldCoordAt(const Vector3& pos, float chunkSize) const
	{
		return {
			(int)std::floor((pos.x - tileCenterOffset.x) / chunkSize),
			(int)std::floor((pos.z - tileCenterOffset.z) / chunkSize)
		};
	}

	// Grid center chunk for a grid of gridSize chunks centered on pos.
	// Odd grids: camera in center chunk, transitions at chunk boundaries.
	// Even grids: camera at grid midpoint (chunk boundary), transitions at chunk midpoints.
	XMINT2 gridCenterAt(const Vector3& pos, float chunkSize, int gridSize) const
	{
		float halfOffset = (gridSize % 2 == 0) ? 0.5f : 0.0f;
		return {
			(int)std::floor((pos.x - tileCenterOffset.x) / chunkSize + halfOffset),
			(int)std::floor((pos.z - tileCenterOffset.z) / chunkSize + halfOffset)
		};
	}
};
