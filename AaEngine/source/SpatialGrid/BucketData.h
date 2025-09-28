// BucketData.h

#pragma once
#include <cstdint>
#include <vector>
#include <limits>
#include "../MathUtils.h"
#include "../ObjectId.h"

using Coord = DirectX::XMINT2;

struct Bucket
{
	uint16_t     groupId = {};										// 0..4095
	Coord        coord{ 0, 0 };                                     // owning cell

	std::vector<Vector3> positions;
	std::vector<ObjectId> ids;
	std::unordered_map<ObjectId, uint32_t> idsMap;

	// Used by a *single* grid to dedupe its dirty queue
	bool dirty = false;

	void reserve(uint32_t cap)
	{
		positions.reserve(cap);
	}
};
