// SpatialGrid.cpp
#include "SpatialGrid.h"

SpatialGrid::SpatialGrid(IBucketProvider& provider,
	float cellWidth,
	uint32_t maxItemsPerBucket)
	: m_provider(provider)
	, m_cellWidth(cellWidth)
	, m_maxItemsPerBucket(maxItemsPerBucket)
{
	if (cellWidth <= 0.f) throw std::invalid_argument("SpatialGrid: cellWidth must be > 0");
	if (maxItemsPerBucket == 0) throw std::invalid_argument("SpatialGrid: maxItemsPerBucket must be > 0");
	m_dirtyQueue.reserve(256);
}

Coord SpatialGrid::toCoord(const Vector3& p) const
{
	const float inv = 1.0f / m_cellWidth;
	int ix = static_cast<int>(std::floor(p.x * inv));
	int iz = static_cast<int>(std::floor(p.z * inv));
	return Coord{ ix, iz };
}

uint16_t SpatialGrid::obtainBucketForCell(const Coord& c)
{
	auto it = m_cellToGroups.find(c);
	if (it != m_cellToGroups.end()) {
		for (uint16_t gid : it->second) {
			Bucket* b = m_provider.GetBucket(gid);
			if (b && b->sizeAlive < m_maxItemsPerBucket) {
				return gid;
			}
		}
	}
	return allocateBucket(c);
}

uint16_t SpatialGrid::allocateBucket(const Coord& c)
{
	uint16_t gid = m_provider.AllocateBucket(c, m_maxItemsPerBucket);
	// register in this grid's cell mapping
	m_cellToGroups[c].push_back(gid);
	markDirty(gid); // structural change => dirty
	return gid;
}

void SpatialGrid::releaseBucket(uint16_t groupId, const Coord& c)
{
	// remove from this grid's mapping
	auto it = m_cellToGroups.find(c);
	if (it != m_cellToGroups.end()) {
		auto& vec = it->second;
		vec.erase(std::remove(vec.begin(), vec.end(), groupId), vec.end());
		if (vec.empty()) m_cellToGroups.erase(it);
	}
	// return to global pool
	m_provider.ReleaseBucket(groupId);
}

void SpatialGrid::markDirty(uint16_t groupId)
{
	Bucket* b = m_provider.GetBucket(groupId);
	if (!b) return;
	if (!b->dirty) {
		b->dirty = true;
		m_dirtyQueue.push_back(groupId);
	}
}

ObjectId SpatialGrid::Add(const Vector3& pos)
{
	Coord c = toCoord(pos);
	uint16_t gid = obtainBucketForCell(c);
	Bucket* b = m_provider.GetBucket(gid);
	if (!b) throw std::runtime_error("SpatialGrid::Add: invalid bucket");

	uint32_t localIdx = 0;
	if (!b->freeList.empty()) {
		localIdx = b->freeList.back(); b->freeList.pop_back();
		if (localIdx >= b->positions.size()) {
			b->positions.resize(localIdx + 1);
			b->alive.resize(localIdx + 1, 0);
		}
		b->positions[localIdx] = pos;
		b->alive[localIdx] = 1;
	}
	else {
		localIdx = static_cast<uint32_t>(b->positions.size());
		b->positions.push_back(pos);
		b->alive.push_back(1);
	}
	b->sizeAlive++;
	markDirty(gid);

	return ObjectId(localIdx, ObjectType::Instanced, gid);
}

bool SpatialGrid::Remove(const ObjectId& id)
{
	uint16_t gid = static_cast<uint16_t>(id.getGroupId());
	uint32_t localIdx = id.getLocalIdx();

	Bucket* b = m_provider.GetBucket(gid);
	if (!b) return false;
	if (localIdx >= b->positions.size()) return false;
	if (!b->alive[localIdx]) return false;

	b->alive[localIdx] = 0;
	b->freeList.push_back(localIdx);
	if (b->sizeAlive > 0) b->sizeAlive--;
	markDirty(gid);

	if (b->sizeAlive == 0) {
		releaseBucket(gid, b->coord);
	}
	return true;
}

bool SpatialGrid::Update(const ObjectId& id, const Vector3& newPos)
{
	uint16_t gid = static_cast<uint16_t>(id.getGroupId());
	uint32_t localIdx = id.getLocalIdx();

	Bucket* b = m_provider.GetBucket(gid);
	if (!b) return false;
	if (localIdx >= b->positions.size()) return false;
	if (!b->alive[localIdx]) return false;

	Coord oldC = b->coord;
	Coord newC = toCoord(newPos);

	if (newC.x == oldC.x && newC.y == oldC.y) {
		b->positions[localIdx] = newPos;
		markDirty(gid);
		return true;
	}

	// Move across cells: remove from old bucket, reinsert
	b->alive[localIdx] = 0;
	b->freeList.push_back(localIdx);
	if (b->sizeAlive > 0) b->sizeAlive--;
	markDirty(gid);

	if (b->sizeAlive == 0) {
		releaseBucket(gid, oldC);
	}

	uint16_t newGid = obtainBucketForCell(newC);
	Bucket* nb = m_provider.GetBucket(newGid);
	if (!nb) return false;

	uint32_t newLocal = 0;
	if (!nb->freeList.empty()) {
		newLocal = nb->freeList.back(); nb->freeList.pop_back();
		if (newLocal >= nb->positions.size()) {
			nb->positions.resize(newLocal + 1);
			nb->alive.resize(newLocal + 1, 0);
		}
		nb->positions[newLocal] = newPos;
		nb->alive[newLocal] = 1;
	}
	else {
		newLocal = static_cast<uint32_t>(nb->positions.size());
		nb->positions.push_back(newPos);
		nb->alive.push_back(1);
	}
	nb->sizeAlive++;
	markDirty(newGid);

	// NOTE: ObjectId a -> (localIdx,gid) is now stale if moved to new bucket.
	return true;
}

std::vector<uint16_t> SpatialGrid::ConsumeDirtyBuckets()
{
	std::vector<uint16_t> out;
	out.swap(m_dirtyQueue);

	auto iter = std::unique(out.begin(), out.end());
	out.erase(iter, out.end());

	// clear per-bucket flag
	for (uint16_t gid : out) {
		Bucket* b = m_provider.GetBucket(gid);
		if (b) b->dirty = false;
	}
	return out;
}
