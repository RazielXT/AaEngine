#include "SpatialGrid.h"
#include <stdexcept>

void SpatialGrid::ensureBucketForCell(CellData& cell) {
	if (cell.buckets.empty() || cell.buckets.back().full()) {
		cell.buckets.emplace_back();
		cell.buckets.back().ids.reserve(kBucketCapacity);
		cell.buckets.back().positions.reserve(kBucketCapacity);
	}
}

void SpatialGrid::eraseEmptyTailBuckets(CellData& cell) {
	// Clean up any empty buckets at the end to avoid unbounded growth.
	while (!cell.buckets.empty() && cell.buckets.back().empty()) {
		cell.buckets.pop_back();
	}
}

SpatialGrid::ItemId SpatialGrid::add(const Vector3& position) {
	const Coord c = coordFor(position);
	CellData& cell = grid_[c];
	ensureBucketForCell(cell);

	Bucket& bucket = cell.buckets.back();
	const std::size_t slot = bucket.size();
	const ItemId id = nextId_++; // generate a new unique ID

	bucket.ids.push_back(id);
	bucket.positions.push_back(position);

	// Register reverse lookup
	index_.emplace(id, ItemRef{ c, static_cast<std::uint16_t>(cell.buckets.size() - 1),
								   static_cast<std::uint16_t>(slot) });

	return id;
}

bool SpatialGrid::remove(ItemId id) noexcept {
	auto it = index_.find(id);
	if (it == index_.end()) {
		return false;
	}
	const ItemRef ref = it->second;

	auto cellIt = grid_.find(ref.coord);
	if (cellIt == grid_.end()) {
		// Inconsistent state (shouldn't happen).
		index_.erase(it);
		return false;
	}
	CellData& cell = cellIt->second;
	if (ref.bucketIdx >= cell.buckets.size()) {
		index_.erase(it);
		return false;
	}
	Bucket& bucket = cell.buckets[ref.bucketIdx];
	if (ref.slotIdx >= bucket.ids.size()) {
		index_.erase(it);
		return false;
	}

	const std::size_t lastIdx = bucket.ids.size() - 1;
	if (ref.slotIdx != lastIdx) {
		// Swap-pop the last element into the removed slot.
		const ItemId movedId = bucket.ids[lastIdx];
		bucket.ids[ref.slotIdx] = movedId;
		bucket.positions[ref.slotIdx] = bucket.positions[lastIdx];

		// Update reverse index for the moved item
		auto movedIt = index_.find(movedId);
		if (movedIt != index_.end()) {
			movedIt->second.slotIdx = static_cast<std::uint16_t>(ref.slotIdx);
		}
	}

	bucket.ids.pop_back();
	bucket.positions.pop_back();

	// Clean up empty bucket tails
	if (bucket.empty()) {
		// If this was the last bucket or trailing empties exist, pop them
		eraseEmptyTailBuckets(cell);
		// If the cell is now empty, remove it from the grid
		if (cell.buckets.empty()) {
			grid_.erase(cellIt);
		}
	}

	index_.erase(it);
	return true;
}

bool SpatialGrid::updatePosition(ItemId id, const Vector3& newPosition) noexcept {
	auto it = index_.find(id);
	if (it == index_.end()) return false;

	ItemRef& ref = it->second;
	auto cellIt = grid_.find(ref.coord);
	if (cellIt == grid_.end()) return false;

	CellData& cell = cellIt->second;
	if (ref.bucketIdx >= cell.buckets.size()) return false;
	Bucket& bucket = cell.buckets[ref.bucketIdx];
	if (ref.slotIdx >= bucket.positions.size()) return false;

	// If the new position stays within the same cell, just update in place.
	const Coord newC = coordFor(newPosition);
	if (newC.gx == ref.coord.gx && newC.gy == ref.coord.gy) {
		bucket.positions[ref.slotIdx] = newPosition;
		return true;
	}

	// Otherwise move the item between cells (keeping its ID stable)
	moveItemBetweenCells(id, newPosition, ref.coord, newC);
	return true;
}

void SpatialGrid::moveItemBetweenCells(ItemId id, const Vector3& pos, const Coord& oldC, const Coord& newC) {
	// Remove from the old bucket (swap-pop) without destroying ID, then insert into new cell.
	auto it = index_.find(id);
	if (it == index_.end()) return;

	ItemRef ref = it->second; // copy
	auto cellIt = grid_.find(oldC);
	if (cellIt == grid_.end()) return;
	CellData& oldCell = cellIt->second;
	if (ref.bucketIdx >= oldCell.buckets.size()) return;
	Bucket& oldBucket = oldCell.buckets[ref.bucketIdx];
	if (ref.slotIdx >= oldBucket.ids.size()) return;

	const std::size_t lastIdx = oldBucket.ids.size() - 1;
	if (ref.slotIdx != lastIdx) {
		// Swap-pop move
		ItemId movedId = oldBucket.ids[lastIdx];
		oldBucket.ids[ref.slotIdx] = movedId;
		oldBucket.positions[ref.slotIdx] = oldBucket.positions[lastIdx];
		// Fix index for the moved element
		auto movedIt = index_.find(movedId);
		if (movedIt != index_.end()) {
			movedIt->second.slotIdx = static_cast<std::uint16_t>(ref.slotIdx);
		}
	}
	oldBucket.ids.pop_back();
	oldBucket.positions.pop_back();

	if (oldBucket.empty()) {
		eraseEmptyTailBuckets(oldCell);
		if (oldCell.buckets.empty()) {
			grid_.erase(cellIt);
		}
	}

	// Insert into the new cell
	CellData& newCell = grid_[newC];
	ensureBucketForCell(newCell);
	Bucket& newBucket = newCell.buckets.back();
	const std::size_t newSlot = newBucket.size();

	newBucket.ids.push_back(id);
	newBucket.positions.push_back(pos);

	// Update index (ID remains the same)
	it->second = ItemRef{ newC, static_cast<std::uint16_t>(newCell.buckets.size() - 1),
						  static_cast<std::uint16_t>(newSlot) };
}

std::optional<SpatialGrid::ItemRef> SpatialGrid::locate(ItemId id) const noexcept {
	auto it = index_.find(id);
	if (it == index_.end()) return std::nullopt;
	return it->second;
}

std::optional<Vector3> SpatialGrid::getPosition(ItemId id) const noexcept {
	auto it = index_.find(id);
	if (it == index_.end()) return std::nullopt;

	const ItemRef& ref = it->second;
	auto cellIt = grid_.find(ref.coord);
	if (cellIt == grid_.end()) return std::nullopt;
	const CellData& cell = cellIt->second;
	if (ref.bucketIdx >= cell.buckets.size()) return std::nullopt;
	const Bucket& bucket = cell.buckets[ref.bucketIdx];
	if (ref.slotIdx >= bucket.positions.size()) return std::nullopt;

	return bucket.positions[ref.slotIdx];
}

const std::vector<SpatialGrid::ItemId>& SpatialGrid::bucketIds(const Coord& c, std::size_t bucketIdx) const {
	const auto cellIt = grid_.find(c);
	if (cellIt == grid_.end()) throw std::out_of_range("Cell not found");
	const CellData& cell = cellIt->second;
	if (bucketIdx >= cell.buckets.size()) throw std::out_of_range("Bucket index out of range");
	return cell.buckets[bucketIdx].ids;
}

const std::vector<Vector3>& SpatialGrid::bucketPositions(const Coord& c, std::size_t bucketIdx) const {
	const auto cellIt = grid_.find(c);
	if (cellIt == grid_.end()) throw std::out_of_range("Cell not found");
	const CellData& cell = cellIt->second;
	if (bucketIdx >= cell.buckets.size()) throw std::out_of_range("Bucket index out of range");
	return cell.buckets[bucketIdx].positions;
}

std::size_t SpatialGrid::bucketCountInCell(const Coord& c) const {
	const auto it = grid_.find(c);
	if (it == grid_.end()) return 0;
	return it->second.buckets.size();
}
