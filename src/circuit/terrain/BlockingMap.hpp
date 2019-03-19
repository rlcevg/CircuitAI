/*
 * BlockingMap.hpp
 *
 *  Created on: Dec 13, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_BLOCKINGMAP_H_
#	error "Don't include this file directly, include BlockingMap.h instead"
#endif

#include "terrain/BlockingMap.h"

#define MAX_BLOCK_VAL		0x1000
#define BLOCK_THRESHOLD		(GRID_RATIO_LOW * GRID_RATIO_LOW * 3 / 4)

namespace circuit {

inline bool SBlockingMap::IsStruct(int x, int z, StructMask structMask) const
{
	return (grid[z * columns + x].notIgnoreMask & static_cast<SM>(structMask));
}

inline bool SBlockingMap::IsBlocked(int x, int z, SM notIgnoreMask) const
{
	const SBlockCell& cell = grid[z * columns + x];
	return (cell.blockerMask & notIgnoreMask) || static_cast<SM>(cell.structMask);
}

inline bool SBlockingMap::IsBlockedLow(int xLow, int zLow, SM notIgnoreMask) const
{
	return (gridLow[zLow * columnsLow + xLow].blockerMask & notIgnoreMask);
}

inline void SBlockingMap::MarkBlocker(int x, int z, StructType structType, SM notIgnoreMask)
{
	SBlockCell& cell = grid[z * columns + x];
	cell.blockerCounts[static_cast<ST>(structType)] = MAX_BLOCK_VAL;
	cell.notIgnoreMask = notIgnoreMask;
	cell.structMask = GetStructMask(structType);
	const SM structMask = static_cast<SM>(cell.structMask);
	cell.blockerMask |= structMask;

	SBlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
	if (cellLow.blockerCounts[static_cast<ST>(structType)]++ == BLOCK_THRESHOLD) {
		cellLow.blockerMask |= structMask;
	}
}

inline void SBlockingMap::AddBlocker(int x, int z, StructType structType)
{
	SBlockCell& cell = grid[z * columns + x];
	if (cell.blockerCounts[static_cast<ST>(structType)]++ == 0) {
		const SM structMask = static_cast<SM>(GetStructMask(structType));
		cell.blockerMask |= structMask;

		SBlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
		if (++cellLow.blockerCounts[static_cast<ST>(structType)] == BLOCK_THRESHOLD) {
			cellLow.blockerMask |= structMask;
		}
	}
}

inline void SBlockingMap::DelBlocker(int x, int z, StructType structType)
{
	SBlockCell& cell = grid[z * columns + x];
	if (--cell.blockerCounts[static_cast<ST>(structType)] == 0) {
		const int notStructMask = ~static_cast<SM>(GetStructMask(structType));
		cell.blockerMask &= notStructMask;

		SBlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
		if (cellLow.blockerCounts[static_cast<ST>(structType)]-- == BLOCK_THRESHOLD) {
			cellLow.blockerMask &= notStructMask;
		}
	}
}

inline void SBlockingMap::AddStruct(int x, int z, StructType structType, SM notIgnoreMask)
{
	SBlockCell& cell = grid[z * columns + x];
	if (cell.blockerCounts[static_cast<ST>(structType)] == 0) {
		SBlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
		if (++cellLow.blockerCounts[static_cast<ST>(structType)] == BLOCK_THRESHOLD) {
			cellLow.blockerMask |= static_cast<SM>(GetStructMask(structType));
		}
	}
	cell.notIgnoreMask = notIgnoreMask;
	cell.structMask = GetStructMask(structType);
}

inline void SBlockingMap::DelStruct(int x, int z, StructType structType, SM notIgnoreMask)
{
	SBlockCell& cell = grid[z * columns + x];
	cell.notIgnoreMask = 0;
	cell.structMask = StructMask::NONE;
	if (cell.blockerCounts[static_cast<ST>(structType)] == 0) {
		SBlockCellLow& cellLow = gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW];
		if (cellLow.blockerCounts[static_cast<ST>(structType)]-- == BLOCK_THRESHOLD) {
			cellLow.blockerMask &= ~static_cast<SM>(GetStructMask(structType));
		}
	}
}

inline bool SBlockingMap::IsInBounds(const int2& r1, const int2& r2) const
{
	return (r1.x >= 0) && (r1.y >= 0) && (r2.x < columns) && (r2.y < rows);
}

inline bool SBlockingMap::IsInBoundsLow(int x, int z) const
{
	return ((unsigned)x < (unsigned)columnsLow) && ((unsigned)z < (unsigned)rowsLow);
}

inline void SBlockingMap::Bound(int2& r1, int2& r2)
{
	r1.x = std::max(r1.x, 0);  r2.x = std::min(r2.x, columns - 1);
	r1.y = std::max(r1.y, 0);  r2.y = std::min(r2.y, rows - 1);
}

inline SBlockingMap::StructMask SBlockingMap::GetStructMask(StructType structType)
{
	return static_cast<StructMask>(1 << static_cast<ST>(structType));
}

} // namespace circuit
