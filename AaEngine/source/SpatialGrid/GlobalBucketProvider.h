// GlobalBucketProvider.h
#pragma once
#include <array>
#include <vector>
#include <memory>
#include <stdexcept>
#include "IBucketProvider.h"

class GlobalBucketProvider final : public IBucketProvider
{
public:
	GlobalBucketProvider();

	uint16_t AllocateBucket(const Coord& c, uint32_t reserveCapacity) override;
	void     ReleaseBucket(uint16_t groupId) override;

	Bucket* GetBucket(uint16_t groupId) override;
	const Bucket* GetBucket(uint16_t groupId) const override;

private:
	std::array<std::unique_ptr<Bucket>, kMaxBuckets> m_buckets;
	std::vector<uint16_t> m_freeIds; // LIFO freelist
};
