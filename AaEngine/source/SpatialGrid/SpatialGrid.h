// SpatialGrid.h
#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <cmath>
#include <stdexcept>

#include "../MathUtils.h"
#include "../ObjectId.h"
#include "BucketData.h"
#include "IBucketProvider.h"

using Coord = DirectX::XMINT2;

// Key hashing for XMINT2
struct XMINT2Hasher {
	size_t operator()(const Coord& c) const noexcept {
		uint32_t x = static_cast<uint32_t>(c.x);
		uint32_t y = static_cast<uint32_t>(c.y);
		uint64_t k = (static_cast<uint64_t>(x) << 32) ^ static_cast<uint64_t>(y * 0x9E3779B1u);
		k ^= (k >> 33);
		k *= 0xff51afd7ed558ccdULL;
		k ^= (k >> 33);
		k *= 0xc4ceb9fe1a85ec53ULL;
		k ^= (k >> 33);
		return static_cast<size_t>(k);
	}
};

// Equality comparator for XMINT2
struct XMINT2Equal {
	bool operator()(const Coord& a, const Coord& b) const noexcept {
		return a.x == b.x && a.y == b.y;
	}
};

class SpatialGrid
{
public:
	// --- XMINT2 hashing + equality (XMINT2 lacks operator==) ---
	struct XMINT2Hasher {
		size_t operator()(const Coord& c) const noexcept {
			uint32_t x = static_cast<uint32_t>(c.x);
			uint32_t y = static_cast<uint32_t>(c.y);
			uint64_t k = (static_cast<uint64_t>(x) << 32) ^ (static_cast<uint64_t>(y) * 0x9E3779B1u);
			k ^= (k >> 33);
			k *= 0xff51afd7ed558ccdULL;
			k ^= (k >> 33);
			k *= 0xc4ceb9fe1a85ec53ULL;
			k ^= (k >> 33);
			return static_cast<size_t>(k);
		}
	};
	struct XMINT2Equal {
		bool operator()(const Coord& a, const Coord& b) const noexcept {
			return a.x == b.x && a.y == b.y;
		}
	};

public:
	SpatialGrid(IBucketProvider& provider,
		float cellWidth,
		uint32_t maxItemsPerBucket);

	// Core operations
	ObjectId Add(const Vector3& pos);
	bool     Remove(const ObjectId& id);
	bool     Update(const ObjectId& id, const Vector3& newPos);

	// Dirty buckets API (per-grid)
	std::vector<uint16_t> ConsumeDirtyBuckets();

	// Access bucket (read-only or mutable) via provider
	const Bucket* GetBucket(uint16_t groupId) const { return m_provider.GetBucket(groupId); }
	Bucket* GetBucket(uint16_t groupId) { return m_provider.GetBucket(groupId); }

	float    CellWidth() const { return m_cellWidth; }
	uint32_t MaxItemsPerBucket() const { return m_maxItemsPerBucket; }

private:
	Coord   toCoord(const Vector3& p) const;
	uint16_t obtainBucketForCell(const Coord& c); // pick a non-full or allocate new
	uint16_t allocateBucket(const Coord& c);
	void     releaseBucket(uint16_t groupId, const Coord& c);
	void     markDirty(uint16_t groupId);

private:
	// Mapping: cell -> list of bucket groupIds in that cell
	std::unordered_map<Coord, std::vector<uint16_t>, XMINT2Hasher, XMINT2Equal> m_cellToGroups;

	// Per-grid dirty queue (deduped by Bucket::dirty)
	std::vector<uint16_t> m_dirtyQueue;

	// Parameters
	IBucketProvider& m_provider;
	float    m_cellWidth = 1.0f;
	uint32_t m_maxItemsPerBucket = 64;
};
