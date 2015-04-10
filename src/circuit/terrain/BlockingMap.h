/*
 * BlockingMap.h
 *
 *  Created on: Dec 13, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_BLOCKINGMAP_H_
#define SRC_CIRCUIT_TERRAIN_BLOCKINGMAP_H_

#include "System/type2.h"

#include <vector>

#define GRID_RATIO_LOW		8
#define STRUCT_BIT(bits)	static_cast<int>(SBlockingMap::StructMask::bits)

namespace circuit {

struct SBlockingMap {
	enum class StructType: int {FACTORY = 0, MEX, ENGY_LOW, ENGY_MID, ENGY_HIGH, PYLON, DEF_LOW, DEF_MID, DEF_HIGH, SPECIAL, NANO, UNKNOWN, TOTAL_COUNT};
	enum class StructMask: int {  FACTORY = 0x0001,     MEX = 0x0002, ENGY_LOW = 0x0004, ENGY_MID = 0x0008,
								ENGY_HIGH = 0x0010,   PYLON = 0x0020,  DEF_LOW = 0x0040,  DEF_MID = 0x0080,
								 DEF_HIGH = 0x0100, SPECIAL = 0x0200,     NANO = 0x0400,  UNKNOWN = 0x0800,
									 NONE = 0x0000,     ALL = 0xFFFF};

	inline bool IsStruct(int x, int z, StructMask structMask);
	inline bool IsBlocked(int x, int z, int notIgnoreMask);
	inline bool IsBlockedLow(int xLow, int zLow, int notIgnoreMask);
	inline void MarkBlocker(int x, int z, StructType structType, int notIgnoreMask);
	inline void AddBlocker(int x, int z, StructType structType);
	inline void RemoveBlocker(int x, int z, StructType structType);
	inline void AddStruct(int x, int z, StructType structType, int notIgnoreMask);
	inline void RemoveStruct(int x, int z, StructType structType, int notIgnoreMask);

	inline bool IsInBounds(const int2& r1, const int2& r2);
	inline bool IsInBoundsLow(int x, int z);
	inline void Bound(int2& r1, int2& r2);

	static inline StructMask GetStructMask(StructType structType);

	struct BlockCell {
		int blockerMask;
		int notIgnoreMask;  // = ~ignoreMask
		StructMask structMask;
		unsigned int blockerCounts[static_cast<int>(StructType::TOTAL_COUNT)];
	};
	std::vector<BlockCell> grid;  // granularity Map::GetWidth / 2,  Map::GetHeight / 2
	int columns;
	int rows;

	struct BlockCellLow {
		int blockerMask;
		unsigned int blockerCounts[static_cast<int>(StructType::TOTAL_COUNT)];
	};
	// TODO: Replace with QuadTree
	std::vector<BlockCellLow> gridLow;  // granularity Map::GetWidth / 16, Map::GetHeight / 16
	int columnsLow;
	int rowsLow;
};

} // namespace circuit

#include "terrain/BlockingMap.hpp"

#endif // SRC_CIRCUIT_TERRAIN_BLOCKINGMAP_H_
