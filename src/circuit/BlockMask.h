/*
 * BlockMask.h
 *
 *  Created on: Dec 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_BLOCKMASK_H_
#define SRC_CIRCUIT_BLOCKMASK_H_

#include "System/type2.h"

#include <vector>

namespace circuit {

class IBlockMask {
public:
	enum class BlockType: char {OPEN = 0, BLOCKED, STRUCT};

	virtual ~IBlockMask();

	int GetXSize();
	int GetZSize();
	const int2& GetStructOffset(int facing);

	inline BlockType GetTypeSouth(int x, int z);
	inline BlockType GetTypeEast(int x, int z);
	inline BlockType GetTypeNorth(int x, int z);
	inline BlockType GetTypeWest(int x, int z);

protected:
	IBlockMask();
	struct BlockRects {
		int2 b1, b2;  // b - blocker rect
		int2 s1, s2;  // s - structure rect
	};
	BlockRects Init(const int2& offset, const int2& bsize, const int2& ssize);

	// TODO: South, East, North, West masks for performance?
	std::vector<BlockType> mask;
	int xsize;  // UnitDef::GetXSize() / 2
	int zsize;  // UnitDef::GetZSize() / 2
	int2 offsetSouth;  // structure's corner offset within mask
	int2 offsetEast;
	int2 offsetNorth;
	int2 offsetWest;
};

inline IBlockMask::BlockType IBlockMask::GetTypeSouth(int x, int z)
{
	return mask[z * xsize + x];
}

inline IBlockMask::BlockType IBlockMask::GetTypeEast(int x, int z)
{
	return mask[x * zsize + (zsize - 1 - z)];
}

inline IBlockMask::BlockType IBlockMask::GetTypeNorth(int x, int z)
{
//	return mask[(zsize - 1 - z) * xsize + (xsize - 1 - x)];
	return mask[(zsize - z) * xsize - 1 - x];
}

inline IBlockMask::BlockType IBlockMask::GetTypeWest(int x, int z)
{
	return mask[(xsize - 1 - x) * zsize + z];
}

} // namespace circuit

#endif // SRC_CIRCUIT_BLOCKMASK_H_
