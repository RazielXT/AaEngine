// IBucketProvider.h
#pragma once
#include <cstdint>
#include <memory>
#include "BucketData.h"

struct IBucketProvider
{
	static constexpr uint32_t kMaxBuckets = 4096;

	virtual ~IBucketProvider() = default;

	// Allocates a new bucket ID globally (0..4095), creates & initializes the bucket.
	// The bucket is associated with a specific cell coord at creation.
	virtual uint16_t AllocateBucket(const Coord& c, uint32_t reserveCapacity) = 0;

	// Releases (destroys) the bucket and returns its groupId to the global free pool.
	virtual void     ReleaseBucket(uint16_t groupId) = 0;

	// Non-owning accessors to storage
	virtual Bucket* GetBucket(uint16_t groupId) = 0;
	virtual const Bucket* GetBucket(uint16_t groupId) const = 0;
};
