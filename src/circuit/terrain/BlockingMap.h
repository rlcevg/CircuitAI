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
#include <map>
#include <string>

#define GRID_RATIO_ALLY		8
#define GRID_RATIO_LOW		8
#define STRUCT_BIT(bits)	static_cast<SBlockingMap::SM>(SBlockingMap::StructMask::bits)

namespace circuit {

struct SBlockingMap {
	enum class StructType: unsigned short {
		FACTORY = 0, MEX, ENGY_LOW, ENGY_MID, ENGY_HIGH, PYLON, DEF_LOW, DEF_MID, DEF_HIGH, SPECIAL, NANO, TERRA, UNKNOWN, _SIZE_
	};
	enum class StructMask: unsigned short {NONE = 0x0000, ALL = 0xFFFF,
		  FACTORY = 0x0001,     MEX = 0x0002, ENGY_LOW = 0x0004, ENGY_MID = 0x0008,
		ENGY_HIGH = 0x0010,   PYLON = 0x0020,  DEF_LOW = 0x0040,  DEF_MID = 0x0080,
		 DEF_HIGH = 0x0100, SPECIAL = 0x0200,     NANO = 0x0400,    TERRA = 0x0800,
		  UNKNOWN = 0x1000};
	using ST = std::underlying_type<StructType>::type;
	using SM = std::underlying_type<StructMask>::type;
	using StructTypes = std::map<std::string, StructType>;
	using StructMasks = std::map<std::string, StructMask>;

	static inline StructTypes& GetStructTypes() { return structTypes; }
	static inline StructMasks& GetStructMasks() { return structMasks; }

	inline bool IsStructed(int x, int z, StructMask structMask) const;  // IsStructed for block
	inline bool IsBlocked(int x, int z, SM notIgnoreMask) const;  // IsBlocked for struct
	inline bool IsBlockedLow(int xLow, int zLow, SM notIgnoreMask) const;
	inline bool IsStruct(int x, int z) const;  // Is blocked by any struct
	inline void MarkBlocker(int x, int z, StructType structType, SM notIgnoreMask);
	inline void AddBlocker(int x, int z, StructType structType);
	inline void DelBlocker(int x, int z, StructType structType);
	inline void AddStruct(int x, int z, StructType structType, SM notIgnoreMask);
	inline void DelStruct(int x, int z, StructType structType, SM notIgnoreMask);

	inline bool IsZoneAlly(int xAlly, int zAlly) const;
	inline void AddZoneAlly(int xAlly, int zAlly);
	inline void DelZoneAlly(int xAlly, int zAlly);
	inline void AddZoneOwn(int xAlly, int zAlly);
	inline void DelZoneOwn(int xAlly, int zAlly);

	inline bool IsInBounds(const int2& r1, const int2& r2) const;
	inline bool IsInBoundsLow(int x, int z) const;
	inline void Bound(int2& r1, int2& r2);
	inline void BoundAlly(int2& r1, int2& r2);

	static inline StructMask GetStructMask(StructType structType);

	static StructTypes structTypes;
	static StructMasks structMasks;

	struct SBlockCell {
		SM blockerMask;
		SM notIgnoreMask;  // = ~ignoreMask
		StructMask structMask;
		unsigned short blockerCounts[static_cast<ST>(StructType::_SIZE_)];
	};
	std::vector<SBlockCell> grid;  // granularity Map::GetWidth / 2,  Map::GetHeight / 2
	int columns;
	int rows;

	struct SBlockCellLow {
		SM blockerMask;
		unsigned short blockerCounts[static_cast<ST>(StructType::_SIZE_)];
	};
	// TODO: Replace with QuadTree
	std::vector<SBlockCellLow> gridLow;  // granularity Map::GetWidth / 16, Map::GetHeight / 16
	int columnsLow;
	int rowsLow;

	struct SBlockCellAlly {
		unsigned short allyCount;
		unsigned short ownCount;
	};
	std::vector<SBlockCellAlly> gridAlly;  // granularity Map::GetWidth / 16, Map::GetHeight / 16
	int columnsAlly;
	int rowsAlly;
};

} // namespace circuit

#include "terrain/BlockingMap.hpp"

#endif // SRC_CIRCUIT_TERRAIN_BLOCKINGMAP_H_
