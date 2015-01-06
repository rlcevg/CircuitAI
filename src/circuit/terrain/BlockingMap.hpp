/*
 * BlockingMap.hpp
 *
 *  Created on: Dec 13, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_BLOCKINGMAP_H_
#	error "Don't include this file directly, include BlockingMap.h instead"
#endif

#include "terrain/BlockingMap.h"

#define MAX_BLOCK_VAL		32000
#define BLOCK_THRESHOLD		(GRID_RATIO_LOW * GRID_RATIO_LOW * 3 / 4)

namespace circuit {

static SBlockingMap::StructMask structTypes[] = {
	SBlockingMap::StructMask::FACTORY,
	SBlockingMap::StructMask::MEX,
	SBlockingMap::StructMask::ENGY_LOW,
	SBlockingMap::StructMask::ENGY_MID,
	SBlockingMap::StructMask::ENGY_HIGH,
	SBlockingMap::StructMask::PYLON,
	SBlockingMap::StructMask::DEF_LOW,
	SBlockingMap::StructMask::DEF_MID,
	SBlockingMap::StructMask::DEF_HIGH,
	SBlockingMap::StructMask::SPECIAL,
	SBlockingMap::StructMask::NANO,
	SBlockingMap::StructMask::UNKNOWN
};

inline bool SBlockingMap::IsStruct(int x, int z, StructMask structMask)
{
	return (grid[z * columns + x].notIgnoreMask & static_cast<int>(structMask));
}

inline bool SBlockingMap::IsBlocked(int x, int z, int notIgnoreMask)
{
	BlockCell& cell = grid[z * columns + x];
	return (cell.blockerMask & notIgnoreMask) || static_cast<int>(cell.structMask);
}

inline bool SBlockingMap::IsBlockedLow(int xLow, int zLow, int notIgnoreMask)
{
	return (gridLow[zLow * columnsLow + xLow].blockerMask & notIgnoreMask);
}

inline void SBlockingMap::MarkBlocker(int x, int z, StructType structType, int notIgnoreMask)
{
	BlockCell& cell = grid[z * columns + x];
	cell.blockerCounts[static_cast<int>(structType)] = MAX_BLOCK_VAL;
	cell.notIgnoreMask = notIgnoreMask;
	cell.structMask = GetStructMask(structType);
	const int structMask = static_cast<int>(cell.structMask);
	cell.blockerMask |= structMask;

	BlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
	if (cellLow.blockerCounts[static_cast<int>(structType)]++ == BLOCK_THRESHOLD) {
		cellLow.blockerMask |= structMask;
	}
}

inline void SBlockingMap::AddBlocker(int x, int z, StructType structType)
{
	BlockCell& cell = grid[z * columns + x];
	if (cell.blockerCounts[static_cast<int>(structType)]++ == 0) {
		const int structMask = static_cast<int>(GetStructMask(structType));
		cell.blockerMask |= structMask;

		BlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
		if (++cellLow.blockerCounts[static_cast<int>(structType)] == BLOCK_THRESHOLD) {
			cellLow.blockerMask |= structMask;
		}
	}
}

inline void SBlockingMap::RemoveBlocker(int x, int z, StructType structType)
{
	BlockCell& cell = grid[z * columns + x];
	if (--cell.blockerCounts[static_cast<int>(structType)] == 0) {
		const int notStructMask = ~static_cast<int>(GetStructMask(structType));
		cell.blockerMask &= notStructMask;

		BlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
		if (cellLow.blockerCounts[static_cast<int>(structType)]-- == BLOCK_THRESHOLD) {
			cellLow.blockerMask &= notStructMask;
		}
	}
}

inline void SBlockingMap::AddStruct(int x, int z, StructType structType, int notIgnoreMask)
{
	BlockCell& cell = grid[z * columns + x];
	if (cell.blockerCounts[static_cast<int>(structType)] == 0) {
		BlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
		if (++cellLow.blockerCounts[static_cast<int>(structType)] == BLOCK_THRESHOLD) {
			cellLow.blockerMask |= static_cast<int>(GetStructMask(structType));
		}
	}
	cell.notIgnoreMask = notIgnoreMask;
	cell.structMask = GetStructMask(structType);
}

inline void SBlockingMap::RemoveStruct(int x, int z, StructType structType, int notIgnoreMask)
{
	BlockCell& cell = grid[z * columns + x];
	cell.notIgnoreMask = 0;
	cell.structMask = StructMask::NONE;
	if (cell.blockerCounts[static_cast<int>(structType)] == 0) {
		BlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
		if (cellLow.blockerCounts[static_cast<int>(structType)]-- == BLOCK_THRESHOLD) {
			cellLow.blockerMask &= ~static_cast<int>(GetStructMask(structType));
		}
	}
}

inline bool SBlockingMap::IsInBounds(const int2& r1, const int2& r2)
{
	return (r1.x >= 0) && (r1.y >= 0) && (r2.x < columns) && (r2.y < rows);
}

inline bool SBlockingMap::IsInBoundsLow(int x, int z)
{
	return (x >= 0) && (z >= 0) && (x < columnsLow) && (z < rowsLow);
}

inline void SBlockingMap::Bound(int2& r1, int2& r2)
{
	r1.x = std::max(r1.x, 0);  r2.x = std::min(r2.x, columns - 1);
	r1.y = std::max(r1.y, 0);  r2.y = std::min(r2.y, rows - 1);
}

inline SBlockingMap::StructMask SBlockingMap::GetStructMask(StructType structType)
{
	return structTypes[static_cast<int>(structType)];
}

} // namespace circuit
