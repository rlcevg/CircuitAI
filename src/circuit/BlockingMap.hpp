/*
 * BlockingMap.hpp
 *
 *  Created on: Dec 13, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_BLOCKINGMAP_H_
#	error "Don't include this file directly, include BlockingMap.h instead"
#endif

#include "BlockingMap.h"

#define MAX_BLOCK_VAL	32000

namespace circuit {

static SBlockingMap::StructMask structTypes[] = {
	SBlockingMap::StructMask::FACTORY,
	SBlockingMap::StructMask::MEX,
	SBlockingMap::StructMask::ENERGY,
	SBlockingMap::StructMask::PYLON,
	SBlockingMap::StructMask::DEF_LOW,
	SBlockingMap::StructMask::DEF_MID,
	SBlockingMap::StructMask::DEF_HIGH,
	SBlockingMap::StructMask::SPECIAL,
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

inline bool SBlockingMap::IsBlockedLow(int x, int z, int ignoreMask)
{
	return false;
//	return (gridLow[z * columnsLow + x] >= (GRID_RATIO_LOW - 1) * (GRID_RATIO_LOW - 1));
}

inline void SBlockingMap::MarkBlocker(int x, int z, StructType structType)
{
	BlockCell& cell = grid[z * columns + x];
	cell.blockerCounts[static_cast<int>(structType)] = MAX_BLOCK_VAL;
	cell.structMask = GetStructMask(structType);
	cell.blockerMask |= static_cast<int>(cell.structMask);
//	gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]++;
}

inline void SBlockingMap::AddBlocker(int x, int z, StructType structType)
{
	BlockCell& cell = grid[z * columns + x];
	if (cell.blockerCounts[static_cast<int>(structType)]++ == 0) {
		cell.blockerMask |= static_cast<int>(GetStructMask(structType));
//		gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]++;
	}
}

inline void SBlockingMap::RemoveBlocker(int x, int z, StructType structType)
{
	BlockCell& cell = grid[z * columns + x];
	if (--cell.blockerCounts[static_cast<int>(structType)] == 0) {
		cell.blockerMask &= ~static_cast<int>(GetStructMask(structType));
//		gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]--;
	}
}

inline void SBlockingMap::AddStruct(int x, int z, StructType structType, int notIgnoreMask)
{
	BlockCell& cell = grid[z * columns + x];
	if (cell.blockerCounts[static_cast<int>(structType)] == 0) {
//		gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]++;
	}
	cell.notIgnoreMask = notIgnoreMask;
	cell.structMask = static_cast<StructMask>(GetStructMask(structType));
}

inline void SBlockingMap::RemoveStruct(int x, int z, StructType structType, int notIgnoreMask)
{
	BlockCell& cell = grid[z * columns + x];
	cell.notIgnoreMask = 0;
	cell.structMask = StructMask::NONE;
	if (cell.blockerCounts[static_cast<int>(structType)] == 0) {
//		gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]--;
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
