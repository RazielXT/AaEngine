// GlobalBucketProvider.cpp
#include "GlobalBucketProvider.h"

GlobalBucketProvider::GlobalBucketProvider()
{
	m_freeIds.reserve(kMaxBuckets);
	for (uint32_t gid = 0; gid < kMaxBuckets; ++gid) {
		m_freeIds.push_back(static_cast<uint16_t>(kMaxBuckets - gid));
	}
}

uint16_t GlobalBucketProvider::AllocateBucket(const Coord& c, uint32_t reserveCapacity)
{
	if (m_freeIds.empty()) {
		throw std::runtime_error("GlobalBucketProvider: out of bucket IDs (4096 exhausted)");
	}

	const uint16_t gid = m_freeIds.back();
	m_freeIds.pop_back();

	auto& slot = m_buckets[gid];
	slot = std::make_unique<Bucket>();
	Bucket* b = slot.get();
	b->groupId = gid;
	b->coord = c;
	b->sizeAlive = 0;
	b->dirty = false;
	b->positions.clear();
	b->alive.clear();
	b->freeList.clear();
	b->reserve(reserveCapacity);

	return gid;
}

void GlobalBucketProvider::ReleaseBucket(uint16_t groupId)
{
	if (groupId >= kMaxBuckets) return;
	m_buckets[groupId].reset();
	m_freeIds.push_back(groupId);
}

Bucket* GlobalBucketProvider::GetBucket(uint16_t groupId)
{
	if (groupId >= kMaxBuckets) return nullptr;
	return m_buckets[groupId].get();
}

const Bucket* GlobalBucketProvider::GetBucket(uint16_t groupId) const
{
	if (groupId >= kMaxBuckets) return nullptr;
	return m_buckets[groupId].get();
}
